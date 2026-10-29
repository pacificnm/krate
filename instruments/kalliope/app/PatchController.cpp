#include "kalliope/app/PatchController.h"

#include <cmath>

#include <QJSEngine>
#include <QLoggingCategory>

#include "kalliope/app/KalliopeController.h"
#include "kalliope/dsp/Engine.h"

Q_LOGGING_CATEGORY(lcPatches, "kalliope.patches")

namespace {
constexpr int kSaveDelayMs = 2000;
constexpr qreal kSameValue = 1e-4; // a panel value this close to the patch's counts as unedited
} // namespace

using namespace kalliope::persistence;

PatchController::PatchController(KalliopeController &panel, kalliope::Engine &engine, const QString &filePath,
                                 QObject *parent)
    : QObject(parent)
    , m_panel(panel)
    , m_engine(engine)
    , m_filePath(filePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    switch (loadPatches(m_filePath, &m_bank)) {
    case LoadResult::Loaded:
        qCInfo(lcPatches) << "loaded patches from" << m_filePath;
        break;
    case LoadResult::Missing:
        // First run: the factory banks, with the panel left as it is (Manual)
        // until a patch is chosen.
        m_bank.manual = true;
        break;
    case LoadResult::Invalid:
        qCWarning(lcPatches) << "could not read" << m_filePath << "- starting with the factory patches";
        break;
    case LoadResult::NewerVersion:
        qCWarning(lcPatches) << m_filePath << "was written by a newer version; saving to .v1 instead";
        m_filePath += QStringLiteral(".v1");
        break;
    }
    // The panel keeps its own saved state (panel.json); the patch only says
    // whether that state still matches it.
    updateEdited();
    connect(&m_panel, &krate::ParamController::paramsChanged, this, &PatchController::updateEdited);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &PatchController::save);
    m_pollTimer.setInterval(30);
    connect(&m_pollTimer, &QTimer::timeout, this, &PatchController::poll);
    m_pollTimer.start();
}

PatchController::~PatchController()
{
    if (m_saveTimer.isActive())
        save();
    s_instance = nullptr;
}

PatchController *PatchController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QString PatchController::name() const
{
    return m_bank.manual ? QStringLiteral("Manual") : m_bank.patches[std::size_t(m_bank.current)].name;
}

void PatchController::save()
{
    m_saveTimer.stop();
    if (!savePatches(m_filePath, m_bank))
        qCWarning(lcPatches) << "could not save patches to" << m_filePath;
}

void PatchController::poll()
{
    // Program Change n: patch n (bank n / 8, number n % 8).
    while (const auto program = m_engine.popProgram())
        if (*program < kPatchCount)
            select(*program);
}

void PatchController::updateEdited()
{
    bool edited = false;
    if (!m_bank.manual) {
        const Patch &patch = m_bank.patches[std::size_t(m_bank.current)];
        for (auto it = patch.values.begin(); it != patch.values.end() && !edited; ++it)
            edited = std::abs(m_panel.value(it.key()) - it.value()) > kSameValue;
    }
    if (edited != m_edited) {
        m_edited = edited;
        emit changed();
    }
}

void PatchController::selectionChanged()
{
    updateEdited();
    emit changed();
    m_saveTimer.start();
}

void PatchController::select(int index)
{
    if (index < 0 || index >= kPatchCount)
        return;
    m_bank.current = index;
    m_bank.manual = false;
    m_pendingBank = -1;
    m_writing = false;
    m_panel.setValues(m_bank.patches[std::size_t(index)].values);
    selectionChanged();
}

void PatchController::pressBank(int bank)
{
    if (bank < 0 || bank >= kBanks)
        return;
    m_pendingBank = bank;
    emit changed();
}

void PatchController::pressPatch(int number)
{
    if (number < 0 || number >= kPatchesPerBank)
        return;
    const int bank = m_pendingBank >= 0 ? m_pendingBank : this->bank();
    const int index = bank * kPatchesPerBank + number;
    if (!m_writing) {
        select(index);
        return;
    }
    // Store the panel into the slot (its name stays; an Init slot gets a numbered one).
    Patch &patch = m_bank.patches[std::size_t(index)];
    for (auto it = patch.values.begin(); it != patch.values.end(); ++it)
        it.value() = m_panel.value(it.key());
    if (patch.name == initPatch().name)
        patch.name = QStringLiteral("Patch %1%2").arg(bank + 1).arg(number + 1);
    m_bank.current = index;
    m_bank.manual = false;
    m_pendingBank = -1;
    m_writing = false;
    updateEdited();
    emit changed();
    save();
    qCInfo(lcPatches) << "wrote patch" << bank + 1 << number + 1 << patch.name;
}

void PatchController::toggleWrite()
{
    m_writing = !m_writing;
    emit changed();
}

void PatchController::pressManual()
{
    m_bank.manual = true;
    m_pendingBank = -1;
    m_writing = false;
    selectionChanged();
}

void PatchController::step(int delta)
{
    select(((m_bank.current + delta) % kPatchCount + kPatchCount) % kPatchCount);
}
