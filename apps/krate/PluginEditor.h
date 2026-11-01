#pragma once

#include <functional>
#include <memory>

#include <QSocketNotifier>
#include <QString>

namespace krate::clap {
class Instance;
}

// A plugin's editor in a window of its own (docs/DAW.md, D7): an X11 window
// (through XWayland on a Wayland desktop) that the plugin's GUI is put into.
// Krate itself stays a native app. Main thread.
class PluginEditor
{
public:
    PluginEditor(krate::clap::Instance &instance, QString title);
    ~PluginEditor();
    PluginEditor(const PluginEditor &) = delete;
    PluginEditor &operator=(const PluginEditor &) = delete;

    bool open(QString *error = nullptr);
    void close();
    bool isOpen() const { return m_window != 0; }
    bool resize(unsigned width, unsigned height); // the plugin asked to
    void raise();
    std::function<void()> onClosed; // the window's close button

private:
    void pump(); // the window's events

    krate::clap::Instance &m_instance;
    QString m_title;
    void *m_display = nullptr;  // Display *
    unsigned long m_window = 0; // Window
    unsigned long m_deleteAtom = 0;
    bool m_created = false; // the plugin's GUI
    bool m_resizable = false;
    unsigned m_width = 0, m_height = 0;
    std::unique_ptr<QSocketNotifier> m_notifier;
};
