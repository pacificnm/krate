#include "PluginEditor.h"

#include "plugin/Clap.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace {
Display *dpy(void *d) { return static_cast<Display *>(d); }
} // namespace

PluginEditor::PluginEditor(krate::clap::Instance &instance, QString title)
    : m_instance(instance)
    , m_title(std::move(title))
{
}

PluginEditor::~PluginEditor()
{
    close();
}

bool PluginEditor::open(QString *error)
{
    const auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        close();
        return false;
    };
    if (isOpen()) {
        raise();
        return true;
    }
    const clap_plugin *plugin = m_instance.plugin();
    const clap_plugin_gui *gui = m_instance.gui();
    if (!gui)
        return fail(QStringLiteral("This plugin has no editor."));
    if (!gui->is_api_supported(plugin, CLAP_WINDOW_API_X11, false))
        return fail(QStringLiteral("This plugin's editor can't go into an X11 window."));
    m_display = XOpenDisplay(nullptr);
    if (!m_display)
        return fail(QStringLiteral("No X display for plugin windows (XWayland isn't running)."));
    if (!gui->create(plugin, CLAP_WINDOW_API_X11, false))
        return fail(QStringLiteral("The plugin couldn't make its editor."));
    m_created = true;
    gui->set_scale(plugin, 1.0);
    std::uint32_t width = 640, height = 480;
    gui->get_size(plugin, &width, &height);
    m_width = std::max<std::uint32_t>(width, 1);
    m_height = std::max<std::uint32_t>(height, 1);
    m_resizable = gui->can_resize(plugin);

    Display *d = dpy(m_display);
    m_window = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, m_width, m_height, 0, 0, BlackPixel(d, DefaultScreen(d)));
    const QByteArray title = m_title.toUtf8();
    XStoreName(d, m_window, title.constData());
    XChangeProperty(d, m_window, XInternAtom(d, "_NET_WM_NAME", False), XInternAtom(d, "UTF8_STRING", False), 8,
                    PropModeReplace, reinterpret_cast<const unsigned char *>(title.constData()), int(title.size()));
    XClassHint klass{const_cast<char *>("krate"), const_cast<char *>("krate")};
    XSetClassHint(d, m_window, &klass);
    Atom deleteAtom = XInternAtom(d, "WM_DELETE_WINDOW", False);
    m_deleteAtom = deleteAtom;
    XSetWMProtocols(d, m_window, &deleteAtom, 1);
    XSelectInput(d, m_window, StructureNotifyMask);
    if (!m_resizable) {
        XSizeHints hints{};
        hints.flags = PMinSize | PMaxSize;
        hints.min_width = hints.max_width = int(m_width);
        hints.min_height = hints.max_height = int(m_height);
        XSetWMNormalHints(d, m_window, &hints);
    }
    clap_window window{};
    window.api = CLAP_WINDOW_API_X11;
    window.x11 = m_window;
    if (!gui->set_parent(plugin, &window))
        return fail(QStringLiteral("The plugin's editor wouldn't go into its window."));
    XMapWindow(d, m_window);
    XFlush(d);
    gui->show(plugin);

    m_notifier = std::make_unique<QSocketNotifier>(ConnectionNumber(d), QSocketNotifier::Read);
    QObject::connect(m_notifier.get(), &QSocketNotifier::activated, m_notifier.get(), [this] { pump(); });
    pump();
    return true;
}

void PluginEditor::pump()
{
    if (!m_display)
        return;
    Display *d = dpy(m_display);
    bool closing = false;
    while (XPending(d)) {
        XEvent event;
        XNextEvent(d, &event);
        if (event.type == ClientMessage && Atom(event.xclient.data.l[0]) == Atom(m_deleteAtom)) {
            closing = true;
        } else if (event.type == ConfigureNotify && m_resizable && event.xconfigure.window == Window(m_window)) {
            std::uint32_t w = std::uint32_t(event.xconfigure.width), h = std::uint32_t(event.xconfigure.height);
            if (w != m_width || h != m_height) {
                const clap_plugin_gui *gui = m_instance.gui();
                gui->adjust_size(m_instance.plugin(), &w, &h);
                if (gui->set_size(m_instance.plugin(), w, h)) {
                    m_width = w;
                    m_height = h;
                }
            }
        }
    }
    if (closing) {
        close();
        if (onClosed)
            onClosed();
    }
}

bool PluginEditor::resize(unsigned width, unsigned height)
{
    if (!isOpen())
        return false;
    Display *d = dpy(m_display);
    m_width = width;
    m_height = height;
    if (!m_resizable) {
        XSizeHints hints{};
        hints.flags = PMinSize | PMaxSize;
        hints.min_width = hints.max_width = int(width);
        hints.min_height = hints.max_height = int(height);
        XSetWMNormalHints(d, m_window, &hints);
    }
    XResizeWindow(d, m_window, width, height);
    XFlush(d);
    return true;
}

void PluginEditor::raise()
{
    if (!isOpen())
        return;
    XRaiseWindow(dpy(m_display), m_window);
    XFlush(dpy(m_display));
}

void PluginEditor::close()
{
    m_notifier.reset();
    if (m_created) {
        const clap_plugin_gui *gui = m_instance.gui();
        gui->hide(m_instance.plugin());
        gui->destroy(m_instance.plugin());
        m_created = false;
    }
    if (m_display) {
        Display *d = dpy(m_display);
        if (m_window)
            XDestroyWindow(d, m_window);
        XFlush(d);
        XCloseDisplay(d);
    }
    m_window = 0;
    m_display = nullptr;
}
