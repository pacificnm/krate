// krunch-fit: Krunch amp heads fitted to amp captures (docs/HEADS.md).
//
//   krunch-fit --di take.wav [--di take2.wav]... [--out dir] [--evals N] [--starts N]
//              [--threads N] [--ir cabinet.wav] [--seed N] capture.nam [capture2.nam]...
//   krunch-fit --di take.wav --target-voice heavy   (a self-test: a built-in head as the target)
//
// For each capture: the dry takes through it (every knob at noon, no
// cabinet) are the target; Krunch's built-in voices with every tube are
// tried, the closest few searched from (their stages, tubes and tone stack
// kept, everything else moved); the best becomes <dir>/<capture name>.json,
// with a report (.txt) and A/B files to listen to (-capture.wav, -head.wav,
// and -ab.wav switching between them every 3 seconds).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <QDateTime>
#include <QJsonArray>
#include <QString>

#include "HeadFit.h"
#include "app/HeadFile.h"
#include "dsp/Capture.h"
#include "dsp/Krunch.h"
#include "dsp/Wav.h"
#include "tools/WavFile.h"

namespace fit = krunch::fit;
using krunch::HeadDesign;

namespace {

int usage()
{
    std::fprintf(stderr, "usage: krunch-fit --di take.wav [--di take2.wav]... [--out dir] [--evals N] [--starts N]\n"
                         "                  [--threads N] [--ir cabinet.wav] [--seed N] capture.nam [capture2.nam]...\n"
                         "       krunch-fit --di take.wav --target-voice clean|crunch|heavy|lead   (self-test)\n");
    return 2;
}

const char *const kVoices[] = {"Clean", "Crunch", "Heavy", "Lead"};
const char *const kTubes[] = {"high gain", "smooth", "medium", "low gain"};
const char *const kPowers[] = {"British pentode", "American beam", "Small pentode", "Big beam"};

struct Candidate
{
    int voice = 0, tube = 0, power = 0;
    HeadDesign head;
    fit::Score score;
};

std::vector<fit::Features> features(const fit::Material &m, const std::function<std::vector<float>(const fit::Window &)> &render)
{
    std::vector<fit::Features> f;
    for (const fit::Window &w : m.windows)
        f.push_back(fit::analyse(render(w), w.toneHz));
    return f;
}

fit::Score scoreHead(const fit::Material &m, const std::vector<fit::Features> &reference, const HeadDesign &head)
{
    return fit::compare(reference, features(m, [&](const fit::Window &w) { return fit::renderHead(head, w, m.leadIn); }));
}

// The amp playing a stretch of guitar, with the cabinet IR when there is one: to listen to.
std::vector<float> listen(const std::vector<float> &di, krunch::CaptureModel *capture, const HeadDesign *head,
                          const krunch::IrKernel *ir)
{
    krunch::Krunch amp;
    amp.setCapture(0, capture);
    amp.setHead(0, head);
    amp.prepare(fit::kRate, krunch::Krunch::kBlock);
    fit::Window w{di, 0.0};
    // The noon settings, the cabinet on with an IR (else off).
    std::vector<float> out = [&] {
        for (std::size_t p = 0; p < krunch::kParams.size(); ++p) {
            const std::string_view k = krunch::kParams[p].key;
            if (k == "master" || k == "sag" || k == "resonance" || k == "ch1_gain" || k == "ch1_bass" || k == "ch1_mid"
                || k == "ch1_treble" || k == "ch1_presence")
                amp.setParam(int(p), 0.5f);
            else if (k == "ch1_cabinet")
                amp.setParam(int(p), ir ? 1.0f : 0.0f);
            else if (k == "ch1_gate" || k == "ch1_loop" || k == "channel" || k == "power_tube")
                amp.setParam(int(p), 0.0f);
        }
        if (ir)
            amp.cabinet().setKernel(krunch::Krunch::cabinetSlot(0, false), ir);
        std::vector<float> o(di.size()), right(krunch::Krunch::kBlock);
        for (std::size_t at = 0; at < di.size(); at += krunch::Krunch::kBlock) {
            const auto frames = std::uint32_t(std::min<std::size_t>(krunch::Krunch::kBlock, di.size() - at));
            float *outputs[2] = {o.data() + at, right.data()};
            const float *inputs[2] = {di.data() + at, di.data() + at};
            amp.process(krate::AudioBlock{outputs, frames, nullptr, 0, nullptr, inputs});
        }
        return o;
    }();
    return out;
}

bool writeMono(const std::string &path, const std::vector<float> &x)
{
    std::vector<float> stereo;
    stereo.reserve(x.size() * 2);
    for (float s : x) {
        stereo.push_back(s);
        stereo.push_back(s);
    }
    return krate::writeStereoWav(path, stereo, int(fit::kRate));
}

} // namespace

int main(int argc, char **argv)
{
    std::vector<std::string> takes, captures;
    std::string outDir = ".", irPath, targetVoice;
    int evaluations = 1500, starts = 3, threads = int(std::max(1u, std::thread::hardware_concurrency()));
    unsigned seed = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--di")
            takes.push_back(next());
        else if (a == "--out")
            outDir = next();
        else if (a == "--evals")
            evaluations = std::max(50, std::atoi(next().c_str()));
        else if (a == "--starts")
            starts = std::max(1, std::atoi(next().c_str()));
        else if (a == "--threads")
            threads = std::max(1, std::atoi(next().c_str()));
        else if (a == "--ir")
            irPath = next();
        else if (a == "--seed")
            seed = unsigned(std::atoi(next().c_str()));
        else if (a == "--target-voice")
            targetVoice = next();
        else if (!a.empty() && a[0] == '-')
            return usage();
        else
            captures.push_back(a);
    }
    if (takes.empty() || (captures.empty() && targetVoice.empty()))
        return usage();

    // The dry takes.
    std::vector<std::vector<float>> di;
    QJsonArray takeNames;
    for (const std::string &path : takes) {
        krate::WavData wav;
        std::string error;
        if (!krate::readWavFile(path, &wav, &error)) {
            std::fprintf(stderr, "could not read %s: %s\n", path.c_str(), error.c_str());
            return 1;
        }
        if (wav.rate != int(fit::kRate)) {
            std::fprintf(stderr, "%s is at %d Hz: record the takes at 48 kHz\n", path.c_str(), wav.rate);
            return 1;
        }
        di.push_back(std::move(wav.left));
        takeNames.append(QString::fromStdString(std::filesystem::path(path).filename().string()));
    }
    const fit::Material material = fit::buildMaterial(di);
    std::printf("material: %zu windows from %zu take(s), DI peak %.1f dBFS; %d threads\n", material.windows.size(), di.size(),
                20.0 * std::log10(std::max(material.diPeak, 1e-9)), threads);

    std::unique_ptr<krunch::IrKernel> ir;
    if (!irPath.empty()) {
        krate::WavData wav;
        std::string error;
        if (!krate::readWavFile(irPath, &wav, &error) || !(ir = krunch::IrKernel::build(wav, fit::kRate, &error))) {
            std::fprintf(stderr, "could not load the IR %s: %s\n", irPath.c_str(), error.c_str());
            return 1;
        }
    }
    std::filesystem::create_directories(outDir);

    if (!targetVoice.empty())
        captures = {"voice:" + targetVoice};
    for (const std::string &capturePath : captures) {
        const auto t0 = std::chrono::steady_clock::now();
        std::unique_ptr<krunch::CaptureModel> capture;
        HeadDesign target;
        std::string name;
        if (capturePath.rfind("voice:", 0) == 0) { // the self-test: a built-in head, high gain tubes, British pentodes
            const std::string v = capturePath.substr(6);
            int voice = 0;
            for (int i = 0; i < 4; ++i)
                if (QString::fromLatin1(kVoices[i]).compare(QString::fromStdString(v), Qt::CaseInsensitive) == 0)
                    voice = i;
            // That voice reshaped at random (its stages, power amp, voicing): an amp the
            // built-ins aren't, which the search must find its way back to.
            const fit::Space space(krunch::explicitHead(krunch::Voice(voice), krunch::PreTube::HighGain, krunch::PowerTube::BritishPentode));
            std::vector<double> x = space.encode(krunch::explicitHead(krunch::Voice(voice), krunch::PreTube::HighGain,
                                                                     krunch::PowerTube::BritishPentode));
            std::mt19937 shake(seed + 99);
            std::normal_distribution<double> normal(0.0, 0.12);
            for (double &v : x)
                v = std::clamp(v + normal(shake), 0.0, 1.0);
            target = space.decode(x);
            name = std::string("self-test ") + kVoices[voice];
        } else {
            std::string error;
            capture = krunch::CaptureModel::load(capturePath, fit::kRate, int(krunch::Krunch::kBlock), &error);
            if (!capture) {
                std::fprintf(stderr, "could not load %s: %s\n", capturePath.c_str(), error.c_str());
                continue;
            }
            if (capture->expectedRate() > 0.0 && std::fabs(capture->expectedRate() - fit::kRate) > 1.0)
                std::printf("note: %s was captured at %.0f Hz (fitted at 48 kHz anyway)\n", capturePath.c_str(), capture->expectedRate());
            name = std::filesystem::path(capturePath).stem().string();
        }
        std::printf("\n== %s\n", name.c_str());
        const std::vector<fit::Features> reference = features(material, [&](const fit::Window &w) {
            return capture ? fit::renderCapture(*capture, w, material.leadIn) : fit::renderHead(target, w, material.leadIn);
        });

        double loudest = -200.0;
        for (const fit::Features &f : reference)
            loudest = std::max(loudest, f.powerDb);
        if (loudest < -60.0) {
            std::fprintf(stderr, "%s is nearly silent with this guitar (%.0f dBFS at most): not a real amp capture? skipped\n",
                         capturePath.c_str(), loudest);
            continue;
        }

        // The built-in voices with every pair of tubes: the closest are where the search starts.
        std::vector<Candidate> scouts;
        for (int v = 0; v < 4; ++v)
            for (int t = 0; t < 4; ++t)
                for (int p = 0; p < 4; ++p)
                    scouts.push_back({v, t, p, krunch::explicitHead(krunch::Voice(v), krunch::PreTube(t), krunch::PowerTube(p)), {}});
        fit::parallelFor(int(scouts.size()), threads, [&](int i) {
            scouts[std::size_t(i)].score = scoreHead(material, reference, scouts[std::size_t(i)].head);
        });
        std::sort(scouts.begin(), scouts.end(), [](const Candidate &a, const Candidate &b) { return a.score.total < b.score.total; });
        const Candidate builtIn = scouts.front();
        std::printf("closest built-in: %s, %s preamp tubes, %s: %.2f\n", kVoices[builtIn.voice], kTubes[builtIn.tube],
                    kPowers[builtIn.power], builtIn.score.total);

        // Search from the closest few (one per voice and preamp tube).
        std::vector<Candidate> chosen;
        for (const Candidate &c : scouts) {
            if (int(chosen.size()) >= starts)
                break;
            if (std::none_of(chosen.begin(), chosen.end(), [&](const Candidate &o) { return o.voice == c.voice && o.tube == c.tube; }))
                chosen.push_back(c);
        }
        std::mt19937 rng(seed);
        Candidate best = builtIn;
        for (const Candidate &start : chosen) {
            const fit::Space space(start.head);
            const auto evaluate = [&](const std::vector<std::vector<double>> &xs) {
                std::vector<double> f(xs.size());
                fit::parallelFor(int(xs.size()), threads, [&](int i) {
                    f[std::size_t(i)] = scoreHead(material, reference, space.decode(xs[std::size_t(i)])).total;
                });
                return f;
            };
            int lastShown = 0;
            const fit::SearchResult r = fit::search(evaluate, space.encode(start.head), 0.05, evaluations, rng, [&](int done, double value) {
                if (done - lastShown >= evaluations / 5) {
                    lastShown = done;
                    std::printf("  from %s / %s: %d tries, best %.2f\n", kVoices[start.voice], kTubes[start.tube], done, value);
                    std::fflush(stdout);
                }
            });
            if (r.bestValue < best.score.total) {
                best = start;
                best.head = space.decode(r.best);
                best.score = scoreHead(material, reference, best.head);
            }
        }
        best.head.outputDb += best.score.levelDb; // as loud as the capture
        best.head.channelTube = false;
        best.head.channelPower = false;
        const fit::Score final = scoreHead(material, reference, best.head);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

        // The head, its report, and something to listen to.
        krunch::HeadFile file;
        file.name = QString::fromStdString(name);
        file.design = best.head;
        file.fit = QJsonObject{
            {QStringLiteral("capture"), QString::fromStdString(std::filesystem::path(capturePath).filename().string())},
            {QStringLiteral("takes"), takeNames},
            {QStringLiteral("start"), QStringLiteral("%1, %2 preamp tubes, %3").arg(kVoices[best.voice], kTubes[best.tube], kPowers[best.power])},
            {QStringLiteral("score"), final.total},
            {QStringLiteral("built_in_score"), builtIn.score.total},
            {QStringLiteral("date"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        };
        const std::string base = (std::filesystem::path(outDir) / name).string();
        file.write(QString::fromStdString(base + ".json"));
        const auto line = [](const char *what, const fit::Score &s) {
            return QStringLiteral("%1 %2 (tone %3 dB, attack %4 dB, envelope %5 dB, harmonics %6 dB apart)\n")
                .arg(QString::fromLatin1(what).leftJustified(18), QString::number(s.total, 'f', 2), QString::number(s.tone, 'f', 2),
                     QString::number(s.attack, 'f', 2), QString::number(s.envelope, 'f', 2), QString::number(s.harmonics, 'f', 2));
        };
        const QString report = QStringLiteral("%1\nfitted to %2 on %3 window(s) from %4 take(s), %5 s\n\nscore (lower is closer):\n")
                                   .arg(QString::fromStdString(name), file.fit.value(u"capture").toString())
                                   .arg(material.windows.size())
                                   .arg(di.size())
                                   .arg(int(seconds))
                               + line("closest built-in", builtIn.score) + line("fitted head", final)
                               + QStringLiteral("\nstarted from %1\n").arg(file.fit.value(u"start").toString());
        if (FILE *f = std::fopen((base + ".txt").c_str(), "w")) {
            std::fputs(report.toUtf8().constData(), f);
            std::fclose(f);
        }
        std::fputs(report.toUtf8().constData(), stdout);

        const std::vector<float> &take = *std::max_element(di.begin(), di.end(), [](auto &a, auto &b) { return a.size() < b.size(); });
        const std::vector<float> excerpt(take.begin(), take.begin() + std::ptrdiff_t(std::min<std::size_t>(take.size(), std::size_t(30 * fit::kRate))));
        const std::vector<float> a = capture ? listen(excerpt, capture.get(), nullptr, ir.get()) : listen(excerpt, nullptr, &target, ir.get());
        const std::vector<float> b = listen(excerpt, nullptr, &best.head, ir.get());
        std::vector<float> ab(a.size());
        const auto every = std::size_t(3 * fit::kRate);
        for (std::size_t i = 0; i < ab.size(); ++i)
            ab[i] = (i / every) % 2 ? b[i] : a[i];
        writeMono(base + "-capture.wav", a);
        writeMono(base + "-head.wav", b);
        writeMono(base + "-ab.wav", ab);
        std::printf("wrote %s.json, .txt, -capture.wav, -head.wav, -ab.wav (capture first, then the head, every 3 s)\n", base.c_str());
    }
    return 0;
}
