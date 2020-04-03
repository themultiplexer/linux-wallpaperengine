#include <iostream>
#include <irrlicht/irrlicht.h>
#include <sstream>
#include <wallpaperengine/video/renderer.h>
#include <wallpaperengine/video/material.h>
#include <wallpaperengine/irr/CPkgReader.h>
#include <getopt.h>
#include <SDL_mixer.h>
#include <SDL.h>

#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <gdk/gdkx.h>

// support for randr extended screens
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "wallpaperengine/shaders/compiler.h"
#include "wallpaperengine/project.h"
#include "wallpaperengine/irrlicht.h"
#include "wallpaperengine/irr/CImageLoaderTEX.h"

GtkWidget *viewport1;

bool IsRootWindow = false;
std::vector<std::string> Screens;
std::vector<irr::core::rect<irr::s32>> Viewports;

irr::SIrrlichtCreationParameters _irr_params;

irr::f32 g_Time = 0;

void initialize_viewports ()
{
    if (IsRootWindow == false || Screens.size () == 0)
        return;

    Display* display = XOpenDisplay (NULL);
    int xrandr_result, xrandr_error;

    if (!XRRQueryExtension (display, &xrandr_result, &xrandr_error))
    {
        std::cerr << "XRandr is not present, cannot detect specified screens, running in window mode" << std::endl;
        return;
    }

    XRRScreenResources* screenResources = XRRGetScreenResources (display, DefaultRootWindow (display));

    // there are some situations where xrandr returns null (like screen not using the extension)
    if (screenResources == nullptr)
        return;

    for (int i = 0; i < screenResources->noutput; i ++)
    {
        XRROutputInfo* info = XRRGetOutputInfo (display, screenResources, screenResources->outputs [i]);

        // there are some situations where xrandr returns null (like screen not using the extension)
        if (info == nullptr)
            continue;

        std::vector<std::string>::iterator cur = Screens.begin ();
        std::vector<std::string>::iterator end = Screens.end ();

        for (; cur != end; cur ++)
        {
            if (info->connection == RR_Connected && strcmp (info->name, (*cur).c_str ()) == 0)
            {
                XRRCrtcInfo* crtc = XRRGetCrtcInfo (display, screenResources, info->crtc);

                std::cout << "Found requested screen: " << info->name << " -> " << crtc->x << "x" << crtc->y << ":" << crtc->width << "x" << crtc->height << std::endl;

                irr::core::rect<irr::s32> viewport;

                viewport.UpperLeftCorner.X = crtc->x;
                viewport.UpperLeftCorner.Y = crtc->y;
                viewport.LowerRightCorner.X = crtc->x + crtc->width;
                viewport.LowerRightCorner.Y = crtc->y + crtc->height;

                Viewports.push_back (viewport);

                XRRFreeCrtcInfo (crtc);
            }
        }

        XRRFreeOutputInfo (info);
    }

    XRRFreeScreenResources (screenResources);


    _irr_params.WindowId = reinterpret_cast<void*> (GDK_WINDOW_XWINDOW(viewport1->window));
}

int init_irrlicht()
{
    // prepare basic configuration for irrlicht
    //_irr_params.AntiAlias = 8;
    //_irr_params.Bits = 16;
    _irr_params.DeviceType = irr::EIDT_X11;
    _irr_params.DriverType = irr::video::EDT_OPENGL;
    _irr_params.Doublebuffer = true;
    //_irr_params.EventReceiver = nullptr;
    //_irr_params.Fullscreen = false;
    //_irr_params.HandleSRGB = false;
    //_irr_params.IgnoreInput = true;
    //_irr_params.Stencilbuffer = true;
    //_irr_params.UsePerformanceTimer = false;
    //_irr_params.Vsync = false;
    _irr_params.WithAlphaChannel = false;
    _irr_params.ZBufferBits = 24;
    _irr_params.LoggingLevel = irr::ELL_DEBUG;

    initialize_viewports ();

    wp::irrlicht::device = irr::createDeviceEx (_irr_params);

    if (wp::irrlicht::device == nullptr)
    {
        return 1;
    }

    //wp::irrlicht::device->setWindowCaption (L"Test game");
    wp::irrlicht::driver = wp::irrlicht::device->getVideoDriver();

    // check for ps and vs support
    if (
            wp::irrlicht::driver->queryFeature (irr::video::EVDF_PIXEL_SHADER_1_1) == false &&
            wp::irrlicht::driver->queryFeature (irr::video::EVDF_ARB_FRAGMENT_PROGRAM_1) == false)
    {
        wp::irrlicht::device->getLogger ()->log ("WARNING: Pixel shaders disabled because of missing driver/hardware support");
    }

    if (
            wp::irrlicht::driver->queryFeature (irr::video::EVDF_VERTEX_SHADER_1_1) == false &&
            wp::irrlicht::driver->queryFeature (irr::video::EVDF_ARB_VERTEX_PROGRAM_1) == false)
    {
        wp::irrlicht::device->getLogger ()->log ("WARNING: Vertex shaders disabled because of missing driver/hardware support");
    }

    return 0;
}

void preconfigure_wallpaper_engine ()
{
    // load the assets from wallpaper engine
    wp::irrlicht::device->getFileSystem ()->addFileArchive ("assets.zip", true, false);

    // register custom loaders
    wp::irrlicht::driver->addExternalImageLoader (new irr::video::CImageLoaderTex ());
    wp::irrlicht::device->getFileSystem ()->addArchiveLoader (new CArchiveLoaderPkg (wp::irrlicht::device->getFileSystem ()));
}

void print_help (const char* route)
{
    std::cout
        << "Usage:" << route << " [options] " << std::endl
        << "options:" << std::endl
        << "  --silent\t\tMutes all the sound the wallpaper might produce" << std::endl
        << "  --dir <folder>\tLoads an uncompressed background from the given <folder>" << std::endl
        << "  --pkg <folder>\tLoads a scene.pkg file from the given <folder>" << std::endl
        << "  --screen-root <screen name>\tDisplay as screen's background" << std::endl
        << "  --fps <maximum-fps>\tLimits the FPS to the given number, useful to keep battery consumption low" << std::endl;
}

std::string stringPathFixes(const std::string& s){
    std::string str(s);
    if(str.empty())
        return s;
    if(str[0] == '\'' && str[str.size() - 1] == '\''){
        str.erase(str.size() - 1, 1);
        str.erase(0,1);
    }
    if(str[str.size() - 1] != '/')
        str += '/';
    return std::move(str);
}

void *Thread(void* arg) {
    gtk_main();
}

int main (int argc, char* argv[])
{
    int mode = 0;
    int max_fps = 30;
    bool audio_support = true;
    std::string path;

    int option_index = 0;

    static struct option long_options [] = {
            {"screen-root", required_argument, 0, 'r'},
            {"pkg",         required_argument, 0, 'p'},
            {"dir",         required_argument, 0, 'd'},
            {"silent",      no_argument,       0, 's'},
            {"help",        no_argument,       0, 'h'},
            {"fps",         required_argument, 0, 'f'},
            {nullptr,              0, 0,   0}
    };

    while (true)
    {
        int c = getopt_long (argc, argv, "r:p:d:shf:", long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 'r':
                IsRootWindow = true;
                Screens.push_back (optarg);
                break;

            case 'p':
                mode = 1;
                path = optarg;
                break;

            case 'd':
                mode = 2;
                path = optarg;
                break;

            case 's':
                audio_support = false;
                break;

            case 'h':
                print_help (argv [0]);
                return 0;

            case 'f':
                max_fps = atoi (optarg);
                break;

            default:
                break;
        }
    }


    gtk_init(&argc, &argv);
    gtk_gl_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_keep_below(GTK_WINDOW(window), true);
    gtk_window_set_decorated(GTK_WINDOW(window), false);
    gtk_widget_set_app_paintable(window, true);
    gtk_widget_set_size_request(window, 1920, 1080);
    gtk_widget_set_can_focus(window, false);
    gtk_window_set_opacity(GTK_WINDOW(window), 0.8);
    gtk_window_set_resizable(GTK_WINDOW(window), false);
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DOCK);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), true);
    gtk_window_stick(GTK_WINDOW(window));

    gtk_window_move(GTK_WINDOW(window), 0, 0);
    
    GtkWidget *fix = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(window), fix);

    GdkGLConfig *glconfig = gdk_gl_config_new_by_mode(static_cast<GdkGLConfigMode>(GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE));

    viewport1 = gtk_drawing_area_new();
    gtk_widget_set_gl_capability (viewport1, glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE);
    gtk_widget_set_size_request(viewport1, 1920, 1080);
    gtk_fixed_put(GTK_FIXED(fix), viewport1, 0, 0);

    gtk_widget_show_all(window);
    gdk_window_input_shape_combine_region(window->window, gdk_region_new(), 0, 0);

    g_signal_connect(window, "destroy",G_CALLBACK(gtk_main_quit), NULL);

    pthread_t tidMov;
    int rc = pthread_create(&tidMov,NULL,Thread,NULL);
    if (rc != 0) {
        fprintf(stderr,"Thread konnte nicht gestartet werden!");
    }


    if (init_irrlicht())
    {
        return 1;
    }

    preconfigure_wallpaper_engine ();

    irr::io::path wallpaper_path;
    irr::io::path project_path;
    irr::io::path scene_path;

    switch (mode)
    {
        case 0:
            print_help (argv [0]);
            return 0;

        // pkg mode
        case 1:
            path = stringPathFixes(path);
            wallpaper_path = wp::irrlicht::device->getFileSystem ()->getAbsolutePath (path.c_str ());
            project_path = wallpaper_path + "project.json";
            scene_path = wallpaper_path + "scene.pkg";

            wp::irrlicht::device->getFileSystem ()->addFileArchive (scene_path, true, false); // add the pkg file to the lookup list
            break;

        // folder mode
        case 2:
            path = stringPathFixes(path);
            wallpaper_path = wp::irrlicht::device->getFileSystem ()->getAbsolutePath (path.c_str ());
            project_path = wallpaper_path + "project.json";

            // set our working directory
            wp::irrlicht::device->getFileSystem ()->changeWorkingDirectoryTo (wallpaper_path);
            break;

        default:
            break;
    }

    if (audio_support == true)
    {
        int mixer_flags = MIX_INIT_MP3 | MIX_INIT_FLAC | MIX_INIT_OGG;

        if (SDL_Init (SDL_INIT_AUDIO) < 0 || mixer_flags != Mix_Init (mixer_flags))
        {
            wp::irrlicht::device->getLogger ()->log ("Cannot initialize SDL audio system", irr::ELL_ERROR);
            return -1;
        }

        // initialize audio engine
        Mix_OpenAudio (22050, AUDIO_S16SYS, 2, 640);
    }

    wp::project* wp_project = new wp::project (project_path);

    if (wp_project->getScene ()->isOrthogonal() == true)
    {
        wp::video::renderer::setupOrthographicCamera (wp_project->getScene ());
    }
    else
    {
        wp::irrlicht::device->getLogger ()->log ("Non-orthogonal cameras not supported yet!!", irr::ELL_ERROR);
        return -2;
    }

    // register nodes
    wp::video::renderer::queueNode (wp_project->getScene ());

    irr::u32 lastTime = 0;
    irr::u32 minimumTime = 1000 / max_fps;
    irr::u32 currentTime = 0;

    irr::u32 startTime = 0;
    irr::u32 endTime = 0;

    while (wp::irrlicht::device->run () && wp::irrlicht::driver)
    {
        // if (device->isWindowActive ())
        {
            currentTime = startTime = wp::irrlicht::device->getTimer ()->getTime ();
            g_Time = currentTime / 1000.0f;

            if (Viewports.size () > 0)
            {
                std::vector<irr::core::rect<irr::s32>>::iterator cur = Viewports.begin ();
                std::vector<irr::core::rect<irr::s32>>::iterator end = Viewports.end ();

                for (; cur != end; cur ++)
                {
                    // change viewport to render to the correct portion of the display
                    wp::irrlicht::driver->setViewPort (*cur);
                    wp::video::renderer::render ();
                }
            }
            else
            {
                wp::video::renderer::render ();
            }

            endTime = wp::irrlicht::device->getTimer ()->getTime ();

            wp::irrlicht::device->sleep (minimumTime - (endTime - startTime), false);
        }
    }

    SDL_Quit ();
    delete wp_project;

    return 0;
}