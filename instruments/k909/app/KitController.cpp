#include "k909/app/KitController.h"

#include <cmath>

#include <QJSEngine>
#include <QLoggingCategory>

#include "k909/app/DrumsController.h"

Q_LOGGING_CATEGORY(lcKits, "k909.kits")

namespace {
constexpr int kSaveDelayMs = 2000;
constexpr qreal kSameValue = 1e-4; // a panel value this close to the kit's counts as unedited
} // namespace

using namespace k909::persistence;

KitController::KitController(DrumsController &panel, const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_panel(panel)
    , m_filePath(filePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    switch (loadKits(m_filePath, &m_bank)) {
    case LoadResult::Loaded:
        qCInfo(lcKits) << "loaded kits from" << m_filePath;
        break;
    case LoadResult::Missing:
        break; // first run: the factory kits; the panel keeps its own state (Classic = the defaults)
    case LoadResult::Invalid:
        qCWarning(lcKits) << "could not read" << m_filePath << "- starting with the factory kits";
        break;
    case LoadResult::NewerVersion:
        qCWarning(lcKits) << m_filePath << "was written by a newer version; saving to .v1 instead";
        m_filePath += QStringLiteral(".v1");
        break;
    }
    m_target = m_bank.current;
    updateEdited();
    connect(&m_panel, &krate::ParamController::paramsChanged, this, &KitController::updateEdited);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &KitController::save);
}

KitController::~KitController()
{
    if (m_saveTimer.isActive())
        save();
    s_instance = nullptr;
}

KitController *KitController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void KitController::save()
{
    m_saveTimer.stop();
    if (!saveKits(m_filePath, m_bank))
        qCWarning(lcKits) << "could not save kits to" << m_filePath;
}

void KitController::updateEdited()
{
    bool edited = false;
    const Kit &kit = m_bank.kits[std::size_t(m_bank.current)];
    for (auto it = kit.values.begin(); it != kit.values.end() && !edited; ++it)
        edited = std::abs(m_panel.value(it.key()) - it.value()) > kSameValue;
    if (edited != m_edited) {
        m_edited = edited;
        emit changed();
    }
}

void KitController::select(int index)
{
    if (index < 0 || index >= kKitCount)
        return;
    m_writing = false;
    m_bank.current = index;
    m_target = index;
    m_panel.setValues(m_bank.kits[std::size_t(index)].values);
    updateEdited();
    emit changed();
    m_saveTimer.start();
}

void KitController::step(int delta)
{
    const auto wrap = [](int i) { return (i % kKitCount + kKitCount) % kKitCount; };
    if (m_writing) {
        m_target = wrap(m_target + delta);
        emit changed();
        return;
    }
    select(wrap(m_bank.current + delta));
}

void KitController::pressWrite()
{
    if (!m_writing) {
        m_writing = true;
        m_target = m_bank.current;
        emit changed();
        return;
    }
    // Store the panel into the target (its name stays; an Init slot gets a numbered one).
    Kit &kit = m_bank.kits[std::size_t(m_target)];
    for (auto it = kit.values.begin(); it != kit.values.end(); ++it)
        it.value() = m_panel.value(it.key());
    if (kit.name == initKit().name)
        kit.name = QStringLiteral("Kit %1").arg(m_target + 1);
    m_bank.current = m_target;
    m_writing = false;
    updateEdited();
    emit changed();
    save();
    qCInfo(lcKits) << "wrote kit" << m_target + 1 << kit.name;
}

void KitController::cancel()
{
    if (!m_writing)
        return;
    m_writing = false;
    m_target = m_bank.current;
    emit changed();
}
