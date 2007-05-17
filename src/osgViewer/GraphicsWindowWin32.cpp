/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under  
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or 
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * OpenSceneGraph Public License for more details.
 *
 * This file is Copyright (C) 2007 - Andr� Garneau (andre@pixdev.com) and licensed under OSGPL.
 *
 * Some elements of GraphicsWindowWin32 have used the Producer implementation as a reference.
 * These elements are licensed under OSGPL as above, with Copyright (C) 2001-2004  Don Burns.
 */

#include <osgViewer/api/Win32/GraphicsWindowWin32>
#include <vector>
#include <map>
#include <sstream>
#include <windowsx.h>

using namespace osgViewer;

namespace osgViewer
{

//
// Defines from the WGL_ARB_pixel_format specification document
// See http://www.opengl.org/registry/specs/ARB/wgl_pixel_format.txt
//

#define WGL_NUMBER_PIXEL_FORMATS_ARB            0x2000
#define WGL_DRAW_TO_WINDOW_ARB                  0x2001
#define WGL_DRAW_TO_BITMAP_ARB                  0x2002
#define WGL_ACCELERATION_ARB                    0x2003
#define WGL_NEED_PALETTE_ARB                    0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB             0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB              0x2006
#define WGL_SWAP_METHOD_ARB                     0x2007
#define WGL_NUMBER_OVERLAYS_ARB                 0x2008
#define WGL_NUMBER_UNDERLAYS_ARB                0x2009
#define WGL_TRANSPARENT_ARB                     0x200A
#define WGL_TRANSPARENT_RED_VALUE_ARB           0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB         0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB          0x2039
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB         0x203A
#define WGL_TRANSPARENT_INDEX_VALUE_ARB         0x203B
#define WGL_SHARE_DEPTH_ARB                     0x200C
#define WGL_SHARE_STENCIL_ARB                   0x200D
#define WGL_SHARE_ACCUM_ARB                     0x200E
#define WGL_SUPPORT_GDI_ARB                     0x200F
#define WGL_SUPPORT_OPENGL_ARB                  0x2010
#define WGL_DOUBLE_BUFFER_ARB                   0x2011
#define WGL_STEREO_ARB                          0x2012
#define WGL_PIXEL_TYPE_ARB                      0x2013
#define WGL_COLOR_BITS_ARB                      0x2014
#define WGL_RED_BITS_ARB                        0x2015
#define WGL_RED_SHIFT_ARB                       0x2016
#define WGL_GREEN_BITS_ARB                      0x2017
#define WGL_GREEN_SHIFT_ARB                     0x2018
#define WGL_BLUE_BITS_ARB                       0x2019
#define WGL_BLUE_SHIFT_ARB                      0x201A
#define WGL_ALPHA_BITS_ARB                      0x201B
#define WGL_ALPHA_SHIFT_ARB                     0x201C
#define WGL_ACCUM_BITS_ARB                      0x201D
#define WGL_ACCUM_RED_BITS_ARB                  0x201E
#define WGL_ACCUM_GREEN_BITS_ARB                0x201F
#define WGL_ACCUM_BLUE_BITS_ARB                 0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB                0x2021
#define WGL_DEPTH_BITS_ARB                      0x2022
#define WGL_STENCIL_BITS_ARB                    0x2023
#define WGL_AUX_BUFFERS_ARB                     0x2024
#define WGL_NO_ACCELERATION_ARB                 0x2025
#define WGL_GENERIC_ACCELERATION_ARB            0x2026
#define WGL_FULL_ACCELERATION_ARB               0x2027
#define WGL_SWAP_EXCHANGE_ARB                   0x2028
#define WGL_SWAP_COPY_ARB                       0x2029
#define WGL_SWAP_UNDEFINED_ARB                  0x202A
#define WGL_TYPE_RGBA_ARB                       0x202B
#define WGL_TYPE_COLORINDEX_ARB                 0x202C
#define WGL_SAMPLE_BUFFERS_ARB                  0x2041
#define WGL_SAMPLES_ARB                         0x2042

//
// Entry points used from the WGL extensions
//
//    BOOL wglChoosePixelFormatARB(HDC hdc,
//                                 const int *piAttribIList,
//                                 const FLOAT *pfAttribFList,
//                                 UINT nMaxFormats,
//                                 int *piFormats,
//                                 UINT *nNumFormats);
//

typedef bool (WINAPI * WGLChoosePixelFormatARB) ( HDC, const int *, const float *, unsigned int, int *, unsigned int * );

//
// Utility class to specify the visual attributes for wglChoosePixelFormatARB() function
//

template <typename T> class WGLAttributes
{
  public:

      WGLAttributes()  {}
      ~WGLAttributes() {}

      void begin()                              { m_parameters.clear(); }
      void set( const T& id, const T& value )   { add(id); add(value); }
      void enable( const T& id )                { add(id); add(true); }
      void disable( const T& id )               { add(id); add(false); }
      void end()                                { add(0); }

      const T* get() const                      { return &m_parameters.front(); }

  protected:

      void add( const T& t )                    { m_parameters.push_back(t); }

      std::vector<T>    m_parameters;        // parameters added

  private:

      // No implementation for these
      WGLAttributes( const WGLAttributes& );
      WGLAttributes& operator=( const WGLAttributes& );
};

typedef WGLAttributes<int>     WGLIntegerAttributes;
typedef WGLAttributes<float> WGLFloatAttributes;

//
// Class responsible for interfacing with the Win32 Window Manager
// The behavior of this class is specific to OSG needs and is not a
// generic Windowing interface.
//
// NOTE: This class is intended to be used by a single-thread.
//         Multi-threading is not enabled for performance reasons.
//         The creation/deletion of graphics windows should be done
//         by a single controller thread. That thread should then
//         call the checkEvents() method of all created windows periodically.
//         This is the case with OSG as a "main" thread does all
//         setup, update & event processing. Rendering is done (optionally) by other threads.
//
// !@todo Have a dedicated thread managed by the Win32WindowingSystem class handle the
//        creation and event message processing for all windows it manages. This
//        is to relieve the "main" thread from having to do this synchronously
//        during frame generation. The "main" thread would only have to process
//          each osgGA-type window event queue.
//

class Win32WindowingSystem : public osg::GraphicsContext::WindowingSystemInterface
{
  public:

    // A class representing an OpenGL rendering context
    class OpenGLContext
    {
      public:

        OpenGLContext()
        : _previousHdc(0),
          _previousHglrc(0),
          _hwnd(0),
          _hdc(0),
          _hglrc(0),
          _restorePreviousOnExit(false)
        {}

        OpenGLContext( HWND hwnd, HDC hdc, HGLRC hglrc )
        : _previousHdc(0),
          _previousHglrc(0),
          _hwnd(hwnd),
          _hdc(hdc),
          _hglrc(hglrc),
          _restorePreviousOnExit(false)
        {}

        ~OpenGLContext();

        void set( HWND hwnd, HDC hdc, HGLRC hglrc )
        {
            _hwnd  = hwnd;
            _hdc   = hdc;
            _hglrc = hglrc;
        }

        HDC deviceContext() { return _hdc; }

        bool makeCurrent( HDC restoreOnHdc, bool restorePreviousOnExit );

      protected:

        //
        // Data members
        //

        HDC   _previousHdc;                // previously HDC to restore rendering context on
        HGLRC _previousHglrc;           // previously current rendering context
        HWND  _hwnd;                    // handle to OpenGL window
        HDC   _hdc;                     // handle to device context
        HGLRC _hglrc;                   // handle to OpenGL rendering context
        bool  _restorePreviousOnExit;   // restore original context on exit

        private:

        // no implementation for these
        OpenGLContext( const OpenGLContext& );
        OpenGLContext& operator=( const OpenGLContext& );
    };

    static std::string osgGraphicsWindowWithCursorClass;    //!< Name of Win32 window class (with cursor) used by OSG graphics window instances
    static std::string osgGraphicsWindowWithoutCursorClass; //!< Name of Win32 window class (without cursor) used by OSG graphics window instances

    Win32WindowingSystem();
    ~Win32WindowingSystem();

    // Access the Win32 windowing system through this singleton class.
    static Win32WindowingSystem* getInterface()
    {
        static Win32WindowingSystem* win32Interface = new Win32WindowingSystem;
        return win32Interface;
    }

    // Return the number of screens present in the system
    virtual unsigned int getNumScreens( const osg::GraphicsContext::ScreenIdentifier& si );

    // Return the resolution of specified screen
    // (0,0) is returned if screen is unknown
    virtual void getScreenResolution( const osg::GraphicsContext::ScreenIdentifier& si, unsigned int& width, unsigned int& height );

    // Return the bits per pixel of specified screen
    // (0) is returned if screen is unknown
    virtual void getScreenColorDepth( const osg::GraphicsContext::ScreenIdentifier& si, unsigned int& dmBitsPerPel );

    // Set the resolution for given screen
    virtual bool setScreenResolution( const osg::GraphicsContext::ScreenIdentifier& si, unsigned int width, unsigned int height );

    // Set the refresh rate for given screen
    virtual bool setScreenRefreshRate( const osg::GraphicsContext::ScreenIdentifier& si, double refreshRate );

    // Return the screen position and width/height.
    // all zeros returned if screen is unknown
    virtual void getScreenPosition( const osg::GraphicsContext::ScreenIdentifier& si, int& originX, int& originY, unsigned int& width, unsigned int& height );

    // Create a graphics context with given traits
    virtual osg::GraphicsContext* createGraphicsContext( osg::GraphicsContext::Traits* traits );

    // Register a newly created native window along with its application counterpart
    // This is required to maintain a link between Windows messages and the application window object
    // at event processing time
    virtual void registerWindow( HWND hwnd, osgViewer::GraphicsWindowWin32* window );

    // Unregister a window
    // This is called as part of a window being torn down
    virtual void unregisterWindow( HWND hwnd );

    // Get the application window object associated with a native window
    virtual osgViewer::GraphicsWindowWin32* getGraphicsWindowFor( HWND hwnd );

    // Return a valid sample OpenGL Device Context and current rendering context that can be used with wglXYZ extensions
    virtual bool getSampleOpenGLContext( OpenGLContext& context, HDC windowHDC, int windowOriginX, int windowOriginY );

  protected:

    // Display devices present in the system
    typedef std::vector<DISPLAY_DEVICE> DisplayDevices;

    // Map Win32 window handles to GraphicsWindowWin32 instance
    typedef std::pair< HWND, osgViewer::GraphicsWindowWin32* >  WindowHandleEntry;
    typedef std::map<  HWND, osgViewer::GraphicsWindowWin32* >  WindowHandles;

    // Enumerate all display devices and return in passed container
    void enumerateDisplayDevices( DisplayDevices& displayDevices ) const;

    // Get the screen device current mode information
    bool getScreenInformation( const osg::GraphicsContext::ScreenIdentifier& si, DISPLAY_DEVICE& displayDevice, DEVMODE& deviceMode );

    // Change the screen settings (resolution, refresh rate, etc.)
    bool changeScreenSettings( const osg::GraphicsContext::ScreenIdentifier& si, DISPLAY_DEVICE& displayDevice, DEVMODE& deviceMode );

    // Register the window classes used by OSG graphics window instances
    void registerWindowClasses();

    // Unregister the window classes used by OSG graphics window instances
    void unregisterWindowClasses();

    // Data members
    WindowHandles       _activeWindows;                //!< handles to active windows
    bool                _windowClassesRegistered;      //!< true after window classes have been registered

 private:

     // No implementation for these
     Win32WindowingSystem( const Win32WindowingSystem& );
     Win32WindowingSystem& operator=( const Win32WindowingSystem& );
};

//
// This is the class we need to create for pbuffers and display devices that are not attached to the desktop
// (and thus cannot have windows created on their surface).
//
// Note its not a GraphicsWindow as it won't need any of the event handling and window mapping facilities.
// 

class GraphicsContextWin32 : public osg::GraphicsContext
{
    public:

        GraphicsContextWin32(osg::GraphicsContext::Traits* traits);
        ~GraphicsContextWin32();
    
        virtual bool valid() const;

        /** Realise the GraphicsContext implementation, 
          * Pure virtual - must be implemented by concrate implementations of GraphicsContext. */
        virtual bool realizeImplementation();

        /** Return true if the graphics context has been realised, and is ready to use, implementation.
          * Pure virtual - must be implemented by concrate implementations of GraphicsContext. */
        virtual bool isRealizedImplementation() const;

        /** Close the graphics context implementation.
          * Pure virtual - must be implemented by concrate implementations of GraphicsContext. */
        virtual void closeImplementation();

        /** Make this graphics context current implementation.
          * Pure virtual - must be implemented by concrate implementations of GraphicsContext. */
        virtual bool makeCurrentImplementation();
          
        /** Make this graphics context current with specified read context implementation.
          * Pure virtual - must be implemented by concrate implementations of GraphicsContext. */
        virtual bool makeContextCurrentImplementation(GraphicsContext* /*readContext*/);

        /** Release the graphics context.*/
        virtual bool releaseContextImplementation();
 
        /** Pure virtual, Bind the graphics context to associated texture implementation.
          * Pure virtual - must be implemented by concrate implementations of GraphicsContext. */
        virtual void bindPBufferToTextureImplementation(GLenum /*buffer*/);

        /** Swap the front and back buffers implementation.
          * Pure virtual - must be implemented by Concrate implementations of GraphicsContext. */
        virtual void swapBuffersImplementation();
        
    protected:

        bool   _valid;
};

//////////////////////////////////////////////////////////////////////////////
//                             Error reporting
//////////////////////////////////////////////////////////////////////////////

static void reportError( const std::string& msg )
{
    osg::notify(osg::WARN) << "Error: " << msg.c_str() << std::endl;
}

static void reportError( const std::string& msg, unsigned int errorCode )
{
    //
    // Some APIs are documented as returning the error in ::GetLastError but apparently do not
    // Skip "Reason" field if the errorCode is still success
    //

    if (errorCode==0)
    {
        reportError(msg);
        return;
    }

    osg::notify(osg::WARN) << "Windows Error #"   << errorCode << ": " << msg.c_str();

    LPVOID lpMsgBuf;

    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      errorCode,
                      0, // Default language
                      (LPTSTR) &lpMsgBuf,
                      0,
                      NULL)!=0)
    {
        osg::notify(osg::WARN) << ". Reason: " << LPTSTR(lpMsgBuf) << std::endl;
        ::LocalFree(lpMsgBuf);
    }
    else
    {
        osg::notify(osg::WARN) << std::endl;
    }
}

static void reportErrorForScreen( const std::string& msg, const osg::GraphicsContext::ScreenIdentifier& si, unsigned int errorCode )
{
    std::ostringstream str;

    str << "[Screen #" << si.screenNum << "] " << msg;
    reportError(str.str(), errorCode);
}

//////////////////////////////////////////////////////////////////////////////
//                       Keyboard key mapping for Win32
//////////////////////////////////////////////////////////////////////////////

class Win32KeyboardMap
{
    public:

        Win32KeyboardMap()
        {
            _keymap[VK_ESCAPE       ] = osgGA::GUIEventAdapter::KEY_Escape;
            _keymap[VK_F1           ] = osgGA::GUIEventAdapter::KEY_F1;
            _keymap[VK_F2           ] = osgGA::GUIEventAdapter::KEY_F2;
            _keymap[VK_F3           ] = osgGA::GUIEventAdapter::KEY_F3;
            _keymap[VK_F4           ] = osgGA::GUIEventAdapter::KEY_F4;
            _keymap[VK_F5           ] = osgGA::GUIEventAdapter::KEY_F5;
            _keymap[VK_F6           ] = osgGA::GUIEventAdapter::KEY_F6;
            _keymap[VK_F7           ] = osgGA::GUIEventAdapter::KEY_F7;
            _keymap[VK_F8           ] = osgGA::GUIEventAdapter::KEY_F8;
            _keymap[VK_F9           ] = osgGA::GUIEventAdapter::KEY_F9;
            _keymap[VK_F10          ] = osgGA::GUIEventAdapter::KEY_F10;
            _keymap[VK_F11          ] = osgGA::GUIEventAdapter::KEY_F11;
            _keymap[VK_F12          ] = osgGA::GUIEventAdapter::KEY_F12;
            _keymap[0xc0            ] = '`';
            _keymap['0'             ] = '0';
            _keymap['1'             ] = '1';
            _keymap['2'             ] = '2';
            _keymap['3'             ] = '3';
            _keymap['4'             ] = '4';
            _keymap['5'             ] = '5';
            _keymap['6'             ] = '6';
            _keymap['7'             ] = '7';
            _keymap['8'             ] = '8';
            _keymap['9'             ] = '9';
            _keymap[0xbd            ] = '-';
            _keymap[0xbb            ] = '=';
            _keymap[VK_BACK         ] = osgGA::GUIEventAdapter::KEY_BackSpace;
            _keymap[VK_TAB          ] = osgGA::GUIEventAdapter::KEY_Tab;
            _keymap['A'             ] = 'A';
            _keymap['B'             ] = 'B';
            _keymap['C'             ] = 'C';
            _keymap['D'             ] = 'D';
            _keymap['E'             ] = 'E';
            _keymap['F'             ] = 'F';
            _keymap['G'             ] = 'G';
            _keymap['H'             ] = 'H';
            _keymap['I'             ] = 'I';
            _keymap['J'             ] = 'J';
            _keymap['K'             ] = 'K';
            _keymap['L'             ] = 'L';
            _keymap['M'             ] = 'M';
            _keymap['N'             ] = 'N';
            _keymap['O'             ] = 'O';
            _keymap['P'             ] = 'P';
            _keymap['Q'             ] = 'Q';
            _keymap['R'             ] = 'R';
            _keymap['S'             ] = 'S';
            _keymap['T'             ] = 'T';
            _keymap['U'             ] = 'U';
            _keymap['V'             ] = 'V';
            _keymap['W'             ] = 'W';
            _keymap['X'             ] = 'X';
            _keymap['Y'             ] = 'Y';
            _keymap['Z'             ] = 'Z';
            _keymap[0xdb            ] = '[';
            _keymap[0xdd            ] = ']';
            _keymap[0xdc            ] = '\\';
            _keymap[VK_CAPITAL      ] = osgGA::GUIEventAdapter::KEY_Caps_Lock;
            _keymap[0xba            ] = ';';
            _keymap[0xde            ] = '\'';
            _keymap[VK_RETURN       ] = osgGA::GUIEventAdapter::KEY_Return;
            _keymap[VK_LSHIFT       ] = osgGA::GUIEventAdapter::KEY_Shift_L;
            _keymap[0xbc            ] = ',';
            _keymap[0xbe            ] = '.';
            _keymap[0xbf            ] = '/';
            _keymap[VK_RSHIFT       ] = osgGA::GUIEventAdapter::KEY_Shift_R;
            _keymap[VK_LCONTROL     ] = osgGA::GUIEventAdapter::KEY_Control_L;
            _keymap[VK_LWIN         ] = osgGA::GUIEventAdapter::KEY_Super_L;
            _keymap[VK_SPACE        ] = ' ';
            _keymap[VK_LMENU        ] = osgGA::GUIEventAdapter::KEY_Alt_L;
            _keymap[VK_RMENU        ] = osgGA::GUIEventAdapter::KEY_Alt_R;
            _keymap[VK_RWIN         ] = osgGA::GUIEventAdapter::KEY_Super_R;
            _keymap[VK_APPS         ] = osgGA::GUIEventAdapter::KEY_Menu;
            _keymap[VK_RCONTROL     ] = osgGA::GUIEventAdapter::KEY_Control_R;
            _keymap[VK_SNAPSHOT     ] = osgGA::GUIEventAdapter::KEY_Print;
            _keymap[VK_SCROLL       ] = osgGA::GUIEventAdapter::KEY_Scroll_Lock;
            _keymap[VK_PAUSE        ] = osgGA::GUIEventAdapter::KEY_Pause;
            _keymap[VK_HOME         ] = osgGA::GUIEventAdapter::KEY_Home;
            _keymap[VK_PRIOR        ] = osgGA::GUIEventAdapter::KEY_Page_Up;
            _keymap[VK_END          ] = osgGA::GUIEventAdapter::KEY_End;
            _keymap[VK_NEXT         ] = osgGA::GUIEventAdapter::KEY_Page_Down;
            _keymap[VK_DELETE       ] = osgGA::GUIEventAdapter::KEY_Delete;
            _keymap[VK_INSERT       ] = osgGA::GUIEventAdapter::KEY_Insert;
            _keymap[VK_LEFT         ] = osgGA::GUIEventAdapter::KEY_Left;
            _keymap[VK_UP           ] = osgGA::GUIEventAdapter::KEY_Up;
            _keymap[VK_RIGHT        ] = osgGA::GUIEventAdapter::KEY_Right;
            _keymap[VK_DOWN         ] = osgGA::GUIEventAdapter::KEY_Down;
            _keymap[VK_NUMLOCK      ] = osgGA::GUIEventAdapter::KEY_Num_Lock;
            _keymap[VK_DIVIDE       ] = osgGA::GUIEventAdapter::KEY_KP_Divide;
            _keymap[VK_MULTIPLY     ] = osgGA::GUIEventAdapter::KEY_KP_Multiply;
            _keymap[VK_SUBTRACT     ] = osgGA::GUIEventAdapter::KEY_KP_Subtract;
            _keymap[VK_ADD          ] = osgGA::GUIEventAdapter::KEY_KP_Add;
            _keymap[VK_NUMPAD7      ] = osgGA::GUIEventAdapter::KEY_KP_Home;
            _keymap[VK_NUMPAD8      ] = osgGA::GUIEventAdapter::KEY_KP_Up;
            _keymap[VK_NUMPAD9      ] = osgGA::GUIEventAdapter::KEY_KP_Page_Up;
            _keymap[VK_NUMPAD4      ] = osgGA::GUIEventAdapter::KEY_KP_Left;
            _keymap[VK_NUMPAD5      ] = osgGA::GUIEventAdapter::KEY_KP_Begin;
            _keymap[VK_NUMPAD6      ] = osgGA::GUIEventAdapter::KEY_KP_Right;
            _keymap[VK_NUMPAD1      ] = osgGA::GUIEventAdapter::KEY_KP_End;
            _keymap[VK_NUMPAD2      ] = osgGA::GUIEventAdapter::KEY_KP_Down;
            _keymap[VK_NUMPAD3      ] = osgGA::GUIEventAdapter::KEY_KP_Page_Down;
            _keymap[VK_NUMPAD0      ] = osgGA::GUIEventAdapter::KEY_KP_Insert;
            _keymap[VK_DECIMAL      ] = osgGA::GUIEventAdapter::KEY_KP_Delete;
            _keymap[VK_CLEAR        ] = osgGA::GUIEventAdapter::KEY_Clear;
        }

        ~Win32KeyboardMap() {}

        int remapKey(int key)
        {
            KeyMap::const_iterator map = _keymap.find(key);
            return map==_keymap.end() ? key : map->second;
        }
      
    protected:

        typedef std::map<int, int> KeyMap;
        KeyMap _keymap;
};

static Win32KeyboardMap s_win32KeyboardMap;
static int remapWin32Key(int key)
{
    return s_win32KeyboardMap.remapKey(key);
}

//////////////////////////////////////////////////////////////////////////////
//         Window procedure for all GraphicsWindowWin32 instances
//           Dispatches the call to the actual instance
//////////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    osgViewer::GraphicsWindowWin32* window = Win32WindowingSystem::getInterface()->getGraphicsWindowFor(hwnd);
    return window ? window->handleNativeWindowingEvent(hwnd, uMsg, wParam, lParam) :
                    ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////
//              Win32WindowingSystem::OpenGLContext implementation
//////////////////////////////////////////////////////////////////////////////

Win32WindowingSystem::OpenGLContext::~OpenGLContext()
{
    if (_restorePreviousOnExit && _previousHglrc!=_hglrc && !::wglMakeCurrent(_previousHdc, _previousHglrc))
    {
        reportError("Win32WindowingSystem::OpenGLContext() - Unable to restore current OpenGL rendering context", ::GetLastError());
    }

    _previousHdc   = 0;
    _previousHglrc = 0;

    if (_hglrc)
    {
        ::wglMakeCurrent(_hdc, NULL);
        ::wglDeleteContext(_hglrc);
        _hglrc = 0;
    }

    if (_hdc)
    {
        ::ReleaseDC(_hwnd, _hdc);
        _hdc = 0;
    }

    if (_hwnd)
    {
        ::DestroyWindow(_hwnd);
        _hwnd = 0;
    }
}

bool Win32WindowingSystem::OpenGLContext::makeCurrent( HDC restoreOnHdc, bool restorePreviousOnExit )
{
    if (_hdc==0 || _hglrc==0) return false;

    _previousHglrc = restorePreviousOnExit ? ::wglGetCurrentContext() : 0;
    _previousHdc   = restoreOnHdc;

    if (_hglrc==_previousHglrc) return true;

    if (!::wglMakeCurrent(_hdc, _hglrc))
    {
        reportError("Win32WindowingSystem::OpenGLContext() - Unable to set current OpenGL rendering context", ::GetLastError());
        return false;
    }

    _restorePreviousOnExit = restorePreviousOnExit;

    return true;
}

//////////////////////////////////////////////////////////////////////////////
//              Win32WindowingSystem implementation
//////////////////////////////////////////////////////////////////////////////

std::string Win32WindowingSystem::osgGraphicsWindowWithCursorClass;
std::string Win32WindowingSystem::osgGraphicsWindowWithoutCursorClass;

Win32WindowingSystem::Win32WindowingSystem()
: _windowClassesRegistered(false)
{
}

Win32WindowingSystem::~Win32WindowingSystem()
{
    unregisterWindowClasses();
}

void Win32WindowingSystem::enumerateDisplayDevices( DisplayDevices& displayDevices ) const
{
    for (unsigned int deviceNum=0;; ++deviceNum)
    {
        DISPLAY_DEVICE displayDevice;
        displayDevice.cb = sizeof(displayDevice);

        if (!::EnumDisplayDevices(NULL, deviceNum, &displayDevice, 0)) break;

        // Do not track devices used for remote access (Terminal Services pseudo-displays, etc.)
        if (displayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) continue;

        // Only return display devices that are attached to the desktop
        if (!(displayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) continue;

        displayDevices.push_back(displayDevice);
    }
}

void Win32WindowingSystem::registerWindowClasses()
{
    if (_windowClassesRegistered) return;

    //
    // Register the window classes used by OSG GraphicsWindowWin32 instances
    //

    std::ostringstream str;
    str << "OSG Graphics Window for Win32 [" << ::GetCurrentProcessId() << "]";

    osgGraphicsWindowWithCursorClass    = str.str() + "{ with cursor }";
    osgGraphicsWindowWithoutCursorClass = str.str() + "{ without cursor }";

    WNDCLASSEX wc;

    HINSTANCE hinst = ::GetModuleHandle(NULL);

    //
    // First class: class for OSG Graphics Window with a cursor enabled
    //

    wc.cbSize        = sizeof(wc);
    wc.style         = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = WindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hinst;
    wc.hIcon         = ::LoadIcon(hinst, "OSG_ICON");
    wc.hCursor       = ::LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = osgGraphicsWindowWithCursorClass.c_str();
    wc.hIconSm       = NULL;

    if (::RegisterClassEx(&wc)==0)
    {
        unsigned int lastError = ::GetLastError();
        if (lastError!=ERROR_CLASS_ALREADY_EXISTS)
        {
            reportError("Win32WindowingSystem::registerWindowClasses() - Unable to register first window class", lastError);
            return;
        }
    }

    //
    // Second class: class for OSG Graphics Window without a cursor
    //

    wc.hCursor       = NULL;
    wc.lpszClassName = osgGraphicsWindowWithoutCursorClass.c_str();

    if (::RegisterClassEx(&wc)==0)
    {
        unsigned int lastError = ::GetLastError();
        if (lastError!=ERROR_CLASS_ALREADY_EXISTS)
        {
            reportError("Win32WindowingSystem::registerWindowClasses() - Unable to register second window class", lastError);
            return;
        }
    }

    _windowClassesRegistered = true;
}

void Win32WindowingSystem::unregisterWindowClasses()
{
    if (_windowClassesRegistered)
    {
        ::UnregisterClass(osgGraphicsWindowWithCursorClass.c_str(),    ::GetModuleHandle(NULL));
        ::UnregisterClass(osgGraphicsWindowWithoutCursorClass.c_str(), ::GetModuleHandle(NULL));
        _windowClassesRegistered = false;
    }
}

bool Win32WindowingSystem::getSampleOpenGLContext( OpenGLContext& context, HDC windowHDC, int windowOriginX, int windowOriginY )
{
    context.set(0, 0, 0);

    registerWindowClasses();

    HWND hwnd = ::CreateWindowEx(WS_EX_OVERLAPPEDWINDOW,
                                 osgGraphicsWindowWithoutCursorClass.c_str(),
                                 NULL,
                                 WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DISABLED,
                                 windowOriginX, 
                                 windowOriginY, 
                                 1, 
                                 1, 
                                 NULL,
                                 NULL,
                                 ::GetModuleHandle(NULL),
                                 NULL);
    if (hwnd==0)
    {
        reportError("Win32WindowingSystem::getSampleOpenGLContext() - Unable to create window", ::GetLastError());
        return false;
    }

    //
    // Set the pixel format of the window
    //

    PIXELFORMATDESCRIPTOR pixelFormat =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL,
        PFD_TYPE_RGBA,
        24,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        24,
        0,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };

    HDC hdc = ::GetDC(hwnd);
    if (hdc==0)
    {
        reportError("Win32WindowingSystem::getSampleOpenGLContext() - Unable to get window device context", ::GetLastError());
        ::DestroyWindow(hwnd);
        return false;
    }

    int pixelFormatIndex = ::ChoosePixelFormat(hdc, &pixelFormat);
    if (pixelFormatIndex==0)
    {
        reportError("Win32WindowingSystem::getSampleOpenGLContext() - Unable to choose pixel format", ::GetLastError());
        ::ReleaseDC(hwnd, hdc);
        ::DestroyWindow(hwnd);
        return false;
    }

    if (!::SetPixelFormat(hdc, pixelFormatIndex, &pixelFormat))
    {
        reportError("Win32WindowingSystem::getSampleOpenGLContext() - Unable to set pixel format", ::GetLastError());
        ::ReleaseDC(hwnd, hdc);
        ::DestroyWindow(hwnd);
        return false;
    }

    HGLRC hglrc = ::wglCreateContext(hdc);
    if (hglrc==0)
    {
        reportError("Win32WindowingSystem::getSampleOpenGLContext() - Unable to create an OpenGL rendering context", ::GetLastError());
        ::ReleaseDC(hwnd, hdc);
        ::DestroyWindow(hwnd);
        return false;
    }

    context.set(hwnd, hdc, hglrc);

    if (!context.makeCurrent(windowHDC, true)) return false;

    return true;
}

unsigned int Win32WindowingSystem::getNumScreens( const osg::GraphicsContext::ScreenIdentifier& si ) 
{
    return si.displayNum==0 ? ::GetSystemMetrics(SM_CMONITORS) : 0;
}

bool Win32WindowingSystem::getScreenInformation( const osg::GraphicsContext::ScreenIdentifier& si, DISPLAY_DEVICE& displayDevice, DEVMODE& deviceMode )
{
    if (si.displayNum>0)
    {
        osg::notify(osg::WARN) << "Win32WindowingSystem::getScreenInformation() - The screen identifier on the Win32 platform must always use display number 0. Value received was " << si.displayNum << std::endl;
        return false;
    }

    DisplayDevices displayDevices;
    enumerateDisplayDevices(displayDevices);

    if (si.screenNum>=displayDevices.size())
    {
        osg::notify(osg::WARN) << "Win32WindowingSystem::getScreenInformation() - Cannot get information for screen " << si.screenNum << " because it does not exist." << std::endl;
        return false;
    }

    displayDevice = displayDevices[si.screenNum];

    deviceMode.dmSize        = sizeof(deviceMode);
    deviceMode.dmDriverExtra = 0;

    if (!::EnumDisplaySettings(displayDevice.DeviceName, ENUM_CURRENT_SETTINGS, &deviceMode))
    {
        std::ostringstream str;
        str << "Win32WindowingSystem::getScreenInformation() - Unable to query information for screen number " << si.screenNum;
        reportError(str.str(), ::GetLastError());
        return false;
    }

    return true;
}

void Win32WindowingSystem::getScreenResolution( const osg::GraphicsContext::ScreenIdentifier& si, unsigned int& width, unsigned int& height )
{
    DISPLAY_DEVICE displayDevice;
    DEVMODE        deviceMode;

    if (getScreenInformation(si, displayDevice, deviceMode))
    {
        width  = deviceMode.dmPelsWidth;
        height = deviceMode.dmPelsHeight;
    }
    else
    {
        width  = 0;
        height = 0;
    }
}

void Win32WindowingSystem::getScreenColorDepth( const osg::GraphicsContext::ScreenIdentifier& si, unsigned int& dmBitsPerPel )
{
    DISPLAY_DEVICE displayDevice;
    DEVMODE        deviceMode;

    if (getScreenInformation(si, displayDevice, deviceMode))
    {
        dmBitsPerPel = deviceMode.dmBitsPerPel;
    }
    else
    {
        dmBitsPerPel  = 0;
    }
}

bool Win32WindowingSystem::changeScreenSettings( const osg::GraphicsContext::ScreenIdentifier& si, DISPLAY_DEVICE& displayDevice, DEVMODE& deviceMode )
{
    //
    // Start by testing if the change would be successful (without applying it)
    //

    unsigned int result = ::ChangeDisplaySettingsEx(displayDevice.DeviceName, &deviceMode, NULL, CDS_TEST, NULL);
    if (result==DISP_CHANGE_SUCCESSFUL)
    {
        result = ::ChangeDisplaySettingsEx(displayDevice.DeviceName, &deviceMode, NULL, 0, NULL);
        if (result==DISP_CHANGE_SUCCESSFUL) return true;
    }

    std::string msg = "Win32WindowingSystem::changeScreenSettings() - Unable to change the screen settings.";

    switch( result )
    {
        case DISP_CHANGE_BADMODE     : msg += " The specified graphics mode is not supported."; break;
        case DISP_CHANGE_FAILED      : msg += " The display driver failed the specified graphics mode."; break;
        case DISP_CHANGE_RESTART     : msg += " The computer must be restarted for the graphics mode to work."; break;
        default : break;
    }

    reportErrorForScreen(msg, si, result);
    return false;
}

bool Win32WindowingSystem::setScreenResolution( const osg::GraphicsContext::ScreenIdentifier& si, unsigned int width, unsigned int height )
{
    DISPLAY_DEVICE displayDevice;
    DEVMODE        deviceMode;

    if (!getScreenInformation(si, displayDevice, deviceMode)) return false;

    deviceMode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;
    deviceMode.dmPelsWidth  = width;
    deviceMode.dmPelsHeight = height;
    
    return changeScreenSettings(si, displayDevice, deviceMode);
}

bool Win32WindowingSystem::setScreenRefreshRate( const osg::GraphicsContext::ScreenIdentifier& si, double refreshRate )
{
    DISPLAY_DEVICE displayDevice;
    DEVMODE        deviceMode;

    unsigned int width, height;
    getScreenResolution(si, width, height);

    if (!getScreenInformation(si, displayDevice, deviceMode)) return false;

    deviceMode.dmFields           = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
    deviceMode.dmPelsWidth        = width;
    deviceMode.dmPelsHeight       = height;
    deviceMode.dmDisplayFrequency = refreshRate;
    
    return changeScreenSettings(si, displayDevice, deviceMode);
}

void Win32WindowingSystem::getScreenPosition( const osg::GraphicsContext::ScreenIdentifier& si, int& originX, int& originY, unsigned int& width, unsigned int& height )
{
    DISPLAY_DEVICE displayDevice;
    DEVMODE        deviceMode;

    if (getScreenInformation(si, displayDevice, deviceMode))
    {
        originX = deviceMode.dmPosition.x;
        originY = deviceMode.dmPosition.y;
        width   = deviceMode.dmPelsWidth;
        height  = deviceMode.dmPelsHeight;
    }
    else
    {
        originX = 0;
        originY = 0;
        width   = 0;
        height  = 0;
    }
}

osg::GraphicsContext* Win32WindowingSystem::createGraphicsContext( osg::GraphicsContext::Traits* traits )
{
    if (traits->pbuffer)
    {
        osg::ref_ptr<osgViewer::GraphicsContextWin32> pbuffer = new GraphicsContextWin32(traits);
        if (pbuffer->valid()) return pbuffer.release();
        else return 0;
    }
    else
    {
        registerWindowClasses();

        osg::ref_ptr<osgViewer::GraphicsWindowWin32> window = new GraphicsWindowWin32(traits);
        if (window->valid()) return window.release();
        else return 0;
    }
}

void Win32WindowingSystem::registerWindow( HWND hwnd, osgViewer::GraphicsWindowWin32* window )
{
    if (hwnd) _activeWindows.insert(WindowHandleEntry(hwnd, window));
}

//
// Unregister a window
// This is called as part of a window being torn down
//

void Win32WindowingSystem::unregisterWindow( HWND hwnd )
{
    if (hwnd) _activeWindows.erase(hwnd);
}

//
// Get the application window object associated with a native window
//

osgViewer::GraphicsWindowWin32* Win32WindowingSystem::getGraphicsWindowFor( HWND hwnd )
{
    WindowHandles::const_iterator entry = _activeWindows.find(hwnd);
    return entry==_activeWindows.end() ? 0 : entry->second;
}

//////////////////////////////////////////////////////////////////////////////
//                    GraphicsWindowWin32 implementation
//////////////////////////////////////////////////////////////////////////////

GraphicsWindowWin32::GraphicsWindowWin32( osg::GraphicsContext::Traits* traits )
: _hwnd(0),
  _hdc(0),
  _hglrc(0),
  _windowProcedure(0),
  _timeOfLastCheckEvents(-1.0),
  _screenOriginX(0),
  _screenOriginY(0),
  _screenWidth(0),
  _screenHeight(0),
  _windowOriginXToRealize(0),
  _windowOriginYToRealize(0),
  _windowWidthToRealize(0),
  _windowHeightToRealize(0),
  _initialized(false),
  _valid(false),
  _realized(false),
  _ownsWindow(true),
  _closeWindow(false),
  _destroyWindow(false),
  _destroying(false)
{
    _traits = traits;

    init();
    
    if (valid())
    {
        setState( new osg::State );
        getState()->setGraphicsContext(this);

        if (_traits.valid() && _traits->sharedContext)
        {
            getState()->setContextID( _traits->sharedContext->getState()->getContextID() );
            incrementContextIDUsageCount( getState()->getContextID() );   
        }
        else
        {
            getState()->setContextID( osg::GraphicsContext::createNewContextID() );
        }
    }
}

GraphicsWindowWin32::~GraphicsWindowWin32()
{
    close();
    destroyWindow();
}

void GraphicsWindowWin32::init()
{
    if (_initialized) return;

    WindowData *windowData = _traits.get() ? dynamic_cast<WindowData*>(_traits->inheritedWindowData.get()) : 0;
    HWND windowHandle = windowData ? windowData->_hwnd : 0;

    _ownsWindow    = windowHandle==0;
    _closeWindow   = false;
    _destroyWindow = false;
    _destroying    = false;

    _initialized = _ownsWindow ? createWindow() : setWindow(windowHandle);
    _valid       = _initialized;
}

bool GraphicsWindowWin32::createWindow()
{
    unsigned int extendedStyle;
    unsigned int windowStyle;

    if (!determineWindowPositionAndStyle(_traits->windowDecoration,
                                         _windowOriginXToRealize,
                                         _windowOriginYToRealize,
                                         _windowWidthToRealize,
                                         _windowHeightToRealize,
                                         windowStyle,
                                         extendedStyle))
    {
        reportError("GraphicsWindowWin32::createWindow() - Unable to determine the window position and style");
        return false;
    }

    _hwnd = ::CreateWindowEx(extendedStyle,
                             _traits->useCursor ? Win32WindowingSystem::osgGraphicsWindowWithCursorClass.c_str() :
                                                  Win32WindowingSystem::osgGraphicsWindowWithoutCursorClass.c_str(),
                             _traits->windowName.c_str(),
                             windowStyle,
                             _windowOriginXToRealize, 
                             _windowOriginYToRealize, 
                             _windowWidthToRealize, 
                             _windowHeightToRealize, 
                             NULL,
                             NULL,
                             ::GetModuleHandle(NULL),
                             NULL);
    if (_hwnd==0)
    {
        reportErrorForScreen("GraphicsWindowWin32::createWindow() - Unable to create window", _traits->screenNum, ::GetLastError());
        return false;
    }

    _hdc = ::GetDC(_hwnd);
    if (_hdc==0)
    {
        reportErrorForScreen("GraphicsWindowWin32::createWindow() - Unable to get window device context", _traits->screenNum, ::GetLastError());
        destroyWindow();
        _hwnd = 0;
        return false;
    }

    //
    // Set the pixel format according to traits specified
    //

    if (!setPixelFormat())
    {
        ::ReleaseDC(_hwnd, _hdc);
        _hdc  = 0;
        destroyWindow();
        return false;
    }

    Win32WindowingSystem::getInterface()->registerWindow(_hwnd, this);
    return true;
}

bool GraphicsWindowWin32::setWindow( HWND handle )
{
    if (_initialized)
    {
        reportErrorForScreen("GraphicsWindowWin32::setWindow() - Window already created; it cannot be changed", _traits->screenNum, ::GetLastError());
        return false;
    }

    if (handle==0)
    {
        reportErrorForScreen("GraphicsWindowWin32::setWindow() - Invalid window handle passed", _traits->screenNum, ::GetLastError());
        return false;
    }

    _hwnd = handle;
    if (_hwnd==0)
    {
        reportErrorForScreen("GraphicsWindowWin32::setWindow() - Unable to retrieve native window handle", _traits->screenNum, ::GetLastError());
        return false;
    }

    _hdc = ::GetDC(_hwnd);
    if (_hdc==0)
    {
        reportErrorForScreen("GraphicsWindowWin32::setWindow() - Unable to get window device context", _traits->screenNum, ::GetLastError());
        _hwnd = 0;
        return false;
    }

    //
    // Check if we must set the pixel format of the inherited window
    //

    if (_traits.valid() && _traits->setInheritedWindowPixelFormat)
    {
        if (!setPixelFormat())
        {
            reportErrorForScreen("GraphicsWindowWin32::setWindow() - Unable to set the inherited window pixel format", _traits->screenNum, ::GetLastError());
            _hdc  = 0;
            _hwnd = 0;
            return false;
        }
    }
    else
    {
        //
        // Create the OpenGL rendering context associated with this window
        //

        _hglrc = ::wglCreateContext(_hdc);
        if (_hglrc==0)
        {
            reportErrorForScreen("GraphicsWindowWin32::setWindow() - Unable to create OpenGL rendering context", _traits->screenNum, ::GetLastError());
            ::ReleaseDC(_hwnd, _hdc);
            _hdc  = 0;
            _hwnd = 0;
            return false;
        }
    }

    if (!registerWindowProcedure())
    {
        ::wglDeleteContext(_hglrc);
        _hglrc = 0;
        ::ReleaseDC(_hwnd, _hdc);
        _hdc  = 0;
        _hwnd = 0;
        return false;
    }

    Win32WindowingSystem::getInterface()->registerWindow(_hwnd, this);

    _initialized = true;
    _valid       = true;

    return true;
}

void GraphicsWindowWin32::destroyWindow( bool deleteNativeWindow )
{
    if (_destroying) return;
    _destroying = true;

    if (_hdc)
    {
        releaseContext();

        if (_hglrc)
        {
            ::wglDeleteContext(_hglrc);
            _hglrc = 0;
        }

        ::ReleaseDC(_hwnd, _hdc);
        _hdc = 0;
    }

    (void)unregisterWindowProcedure();

    if (_hwnd)
    {
        Win32WindowingSystem::getInterface()->unregisterWindow(_hwnd);
        if (_ownsWindow && deleteNativeWindow) ::DestroyWindow(_hwnd);
        _hwnd = 0;
    }

    _initialized = false;
    _realized    = false;
    _valid       = false;
    _destroying  = false;
}

bool GraphicsWindowWin32::registerWindowProcedure()
{
    ::SetLastError(0);
    _windowProcedure = (WNDPROC)::SetWindowLongPtr(_hwnd, GWLP_WNDPROC, LONG_PTR(WindowProc));
    unsigned int error = ::GetLastError();

    if (_windowProcedure==0 && error)
    {
        reportErrorForScreen("GraphicsWindowWin32::registerWindowProcedure() - Unable to register window procedure", _traits->screenNum, error);
        return false;
    }

    return true;
}

bool GraphicsWindowWin32::unregisterWindowProcedure()
{
    if (_windowProcedure==0 || _hwnd==0) return true;

    ::SetLastError(0);
    WNDPROC wndProc = (WNDPROC)::SetWindowLongPtr(_hwnd, GWLP_WNDPROC, LONG_PTR(_windowProcedure));
    unsigned int error = ::GetLastError();

    if (wndProc==0 && error)
    {
        reportErrorForScreen("GraphicsWindowWin32::unregisterWindowProcedure() - Unable to unregister window procedure", _traits->screenNum, error);
        return false;
    }

    _windowProcedure = 0;

    return true;
}

bool GraphicsWindowWin32::determineWindowPositionAndStyle( bool decorated, int& x, int& y, unsigned int& w, unsigned int& h, unsigned int& style, unsigned int& extendedStyle )
{
    if (_traits==0) return false;

    //
    // Query the screen position and size
    //

    osg::GraphicsContext::ScreenIdentifier screenId(_traits->screenNum);
    Win32WindowingSystem* windowManager = Win32WindowingSystem::getInterface();

    windowManager->getScreenPosition(screenId, _screenOriginX, _screenOriginY, _screenWidth, _screenHeight);
    if (_screenWidth==0 || _screenHeight==0) return false;

    x = _traits->x + _screenOriginX;
    y = _traits->y + _screenOriginY;
    w = _traits->width;
    h = _traits->height;

    style = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

    extendedStyle = 0;

    if (decorated)
    {
        style |= WS_CAPTION     |
                 WS_SYSMENU     |
                 WS_MINIMIZEBOX |
                 WS_MAXIMIZEBOX;

        if (_traits->supportsResize) style |= WS_SIZEBOX;

        extendedStyle = WS_EX_APPWINDOW           |
                        WS_EX_OVERLAPPEDWINDOW |
                        WS_EX_ACCEPTFILES      |
                        WS_EX_LTRREADING;

        RECT corners;

        corners.left   = x;
        corners.top    = y;
        corners.right  = x + w - 1;
        corners.bottom = y + h - 1;

        //
        // Determine the location of the window corners in order to have
        // a client area of the requested size
        //

        if (!::AdjustWindowRectEx(&corners, style, FALSE, extendedStyle))
        {
            reportErrorForScreen("GraphicsWindowWin32::determineWindowPositionAndStyle() - Unable to adjust window rectangle", _traits->screenNum, ::GetLastError());
            return false;
        }

        x = corners.left;
        y = corners.top;
        w = corners.right  - corners.left + 1;
        h = corners.bottom - corners.top  + 1;
    }

    return true;
}

static void PreparePixelFormatSpecifications( const osg::GraphicsContext::Traits& traits,
                                              WGLIntegerAttributes&               attributes,
                                              bool                                allowSwapExchangeARB )
{
    attributes.begin();

    attributes.enable(WGL_DRAW_TO_WINDOW_ARB);
    attributes.enable(WGL_SUPPORT_OPENGL_ARB);

    attributes.set(WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB);
    attributes.set(WGL_PIXEL_TYPE_ARB,   WGL_TYPE_RGBA_ARB);

    attributes.set(WGL_COLOR_BITS_ARB,   traits.red + traits.green + traits.blue);
    attributes.set(WGL_RED_BITS_ARB,     traits.red);
    attributes.set(WGL_GREEN_BITS_ARB,   traits.green);
    attributes.set(WGL_BLUE_BITS_ARB,    traits.blue);
    attributes.set(WGL_DEPTH_BITS_ARB,   traits.depth);

    if (traits.doubleBuffer)
    {
        attributes.enable(WGL_DOUBLE_BUFFER_ARB);
        if (allowSwapExchangeARB) attributes.set(WGL_SWAP_METHOD_ARB, WGL_SWAP_EXCHANGE_ARB);
    }

    if (traits.alpha)         attributes.set(WGL_ALPHA_BITS_ARB,     traits.alpha);
    if (traits.stencil)       attributes.set(WGL_STENCIL_BITS_ARB,   traits.stencil);
    if (traits.sampleBuffers) attributes.set(WGL_SAMPLE_BUFFERS_ARB, traits.sampleBuffers);
    if (traits.samples)       attributes.set(WGL_SAMPLES_ARB,        traits.samples);

    if (traits.quadBufferStereo) attributes.enable(WGL_STEREO_ARB);

    attributes.end();
}

static int ChooseMatchingPixelFormat( HDC hdc, int screenNum, const WGLIntegerAttributes& formatSpecifications ,osg::GraphicsContext::Traits* _traits)
{
    //
    // Access the entry point for the wglChoosePixelFormatARB function
    //

    WGLChoosePixelFormatARB wglChoosePixelFormatARB = (WGLChoosePixelFormatARB)wglGetProcAddress("wglChoosePixelFormatARB");
    if (wglChoosePixelFormatARB==0)
    {
        // = openGLContext.getTraits()
        reportErrorForScreen("ChooseMatchingPixelFormat() - wglChoosePixelFormatARB extension not found, trying GDI", screenNum, ::GetLastError());
        PIXELFORMATDESCRIPTOR pixelFormat = { 
            sizeof(PIXELFORMATDESCRIPTOR),  //  size of this pfd 
            1,                     // version number 
            PFD_DRAW_TO_WINDOW |   // support window 
            PFD_SUPPORT_OPENGL |   // support OpenGL 
            (_traits->doubleBuffer ? PFD_DOUBLEBUFFER : NULL),      // double buffered ?
            PFD_TYPE_RGBA,         // RGBA type 
            _traits->red + _traits->green + _traits->blue,                // color depth
            _traits->red ,0, _traits->green ,0, _traits->blue, 0,          // shift bits ignored 
            _traits->alpha,          // alpha buffer ?
            0,                     // shift bit ignored 
            0,                     // no accumulation buffer 
            0, 0, 0, 0,            // accum bits ignored 
            _traits->depth,          // 32 or 16 bit z-buffer ?
            _traits->stencil,        // stencil buffer ?
            0,                     // no auxiliary buffer 
            PFD_MAIN_PLANE,        // main layer 
            0,                     // reserved 
            0, 0, 0                // layer masks ignored 
        }; 
        int pixelFormatIndex = ::ChoosePixelFormat(hdc, &pixelFormat);
        if (pixelFormatIndex == 0)
        {
            reportErrorForScreen("ChooseMatchingPixelFormat() - GDI ChoosePixelFormat Failed.", screenNum, ::GetLastError());
            return -1;
        }

        ::DescribePixelFormat(hdc, pixelFormatIndex ,sizeof(PIXELFORMATDESCRIPTOR),&pixelFormat);
        if (((pixelFormat.dwFlags & PFD_GENERIC_FORMAT) != 0)  && ((pixelFormat.dwFlags & PFD_GENERIC_ACCELERATED) == 0))
        {
            osg::notify(osg::WARN) << "Rendering in software: pixelFormatIndex " << pixelFormatIndex << std::endl;
        }
        return pixelFormatIndex;
    }

    int pixelFormatIndex = 0;
    unsigned int numMatchingPixelFormats = 0;

    if (!wglChoosePixelFormatARB(hdc,
                                 formatSpecifications.get(),
                                 NULL,
                                 1,
                                 &pixelFormatIndex,
                                 &numMatchingPixelFormats))
    {
        reportErrorForScreen("ChooseMatchingPixelFormat() - Unable to choose the requested pixel format", screenNum, ::GetLastError());
        return -1;
    }

    return numMatchingPixelFormats==0 ? -1 : pixelFormatIndex;
}

bool GraphicsWindowWin32::setPixelFormat()
{
    Win32WindowingSystem::OpenGLContext openGLContext;
    if (!Win32WindowingSystem::getInterface()->getSampleOpenGLContext(openGLContext, _hdc, _screenOriginX, _screenOriginY)) return false;

    //
    // Build the specifications of the requested pixel format
    //

    WGLIntegerAttributes formatSpecs;
    ::PreparePixelFormatSpecifications(*_traits, formatSpecs, true);

    //
    // Choose the closest matching pixel format from the specified traits
    //

    int pixelFormatIndex = ::ChooseMatchingPixelFormat(openGLContext.deviceContext(), _traits->screenNum, formatSpecs,_traits.get());

    if (pixelFormatIndex<0)
    {
            unsigned int bpp;
            Win32WindowingSystem::getInterface()->getScreenColorDepth(*_traits.get(), bpp);
            if (bpp < 32) {
                osg::notify(osg::INFO)    << "GraphicsWindowWin32::setPixelFormat() - Display setting is not 32 bit colors, "
                                        << bpp
                                        << " bits per pixel on screen #"
                                        << _traits->screenNum
                                        << std::endl;

                _traits->red = bpp / 4; //integer devide, determine minimum number of bits we will accept
                _traits->green = bpp / 4;
                _traits->blue = bpp / 4;
                ::PreparePixelFormatSpecifications(*_traits, formatSpecs, true);// try again with WGL_SWAP_METHOD_ARB
                pixelFormatIndex = ::ChooseMatchingPixelFormat(openGLContext.deviceContext(), _traits->screenNum, formatSpecs,_traits.get());
            }
    }
    if (pixelFormatIndex<0)
    {
        ::PreparePixelFormatSpecifications(*_traits, formatSpecs, false);
        pixelFormatIndex = ::ChooseMatchingPixelFormat(openGLContext.deviceContext(), _traits->screenNum, formatSpecs,_traits.get());
        if (pixelFormatIndex<0)
        {
            reportErrorForScreen("GraphicsWindowWin32::setPixelFormat() - No matching pixel format found based on traits specified", _traits->screenNum, 0);
            return false;
        }

        osg::notify(osg::INFO) << "GraphicsWindowWin32::setPixelFormat() - Found a matching pixel format but without the WGL_SWAP_METHOD_ARB specification for screen #"
                               << _traits->screenNum
                               << std::endl;
    }

    //
    // Set the pixel format found
    //

    PIXELFORMATDESCRIPTOR pfd;
    ::memset(&pfd, 0, sizeof(pfd));
    pfd.nSize    = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;

    if (!::SetPixelFormat(_hdc, pixelFormatIndex, &pfd))
    {
        reportErrorForScreen("GraphicsWindowWin32::setPixelFormat() - Unable to set pixel format", _traits->screenNum, ::GetLastError());
        return false;
    }

    //
    // Create the OpenGL rendering context associated with this window
    //

    _hglrc = ::wglCreateContext(_hdc);
    if (_hglrc==0)
    {
        reportErrorForScreen("GraphicsWindowWin32::setPixelFormat() - Unable to create OpenGL rendering context", _traits->screenNum, ::GetLastError());
        return false;
    }

    return true;
}

void GraphicsWindowWin32::setWindowDecoration( bool decorated )
{
    unsigned int windowStyle;
    unsigned int extendedStyle;

    //
    // Determine position and size of window with/without decorations to retain the size specified in traits
    //

    int x, y;
    unsigned int w, h;

    if (!determineWindowPositionAndStyle(decorated, x, y, w, h, windowStyle, extendedStyle))
    {
        reportErrorForScreen("GraphicsWindowWin32::setWindowDecoration() - Unable to determine the window position and style", _traits->screenNum, 0);
        return;
    }

    //
    // Change the window style
    //

    ::SetLastError(0);
    unsigned int result = ::SetWindowLong(_hwnd, GWL_STYLE, windowStyle);
    unsigned int error  = ::GetLastError();
    if (result==0 && error)
    {
        reportErrorForScreen("GraphicsWindowWin32::setWindowDecoration() - Unable to set window style", _traits->screenNum, error);
        return;
    }

    //
    // Change the window extended style
    //

    ::SetLastError(0);
    result = ::SetWindowLong(_hwnd, GWL_EXSTYLE, extendedStyle);
    error  = ::GetLastError();
    if (result==0 && error)
    {
        reportErrorForScreen("GraphicsWindowWin32::setWindowDecoration() - Unable to set window extented style", _traits->screenNum, error);
        return;
    }

    //
    // Change the window position and size and realize the style changes
    //

    if (!::SetWindowPos(_hwnd, HWND_TOP, x, y, w, h, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_SHOWWINDOW))
    {
        reportErrorForScreen("GraphicsWindowWin32::setWindowDecoration() - Unable to set new window position and size", _traits->screenNum, ::GetLastError());
        return;
    }

    //
    // Repaint the desktop to cleanup decorations removed
    //

    if (!decorated)
    {
        ::InvalidateRect(NULL, NULL, TRUE);
    }
}

bool GraphicsWindowWin32::realizeImplementation()
{
    if (_realized) return true;

    if (!_initialized)
    {
        init();
        if (!_initialized) return false;
        
        if (_traits.valid() && _traits->sharedContext)
        {
            GraphicsWindowWin32* sharedContextWin32 = dynamic_cast<GraphicsWindowWin32*>(_traits->sharedContext);
            if (sharedContextWin32)
            {
                if (!makeCurrent()) return false;        
                if (!wglShareLists(sharedContextWin32->getWGLContext(), getWGLContext()))
                {
                    reportErrorForScreen("GraphicsWindowWin32::realizeImplementation() - Unable to share OpenGL context", _traits->screenNum, ::GetLastError());
                    return false;
                }
            }
        }
    }

    if (_ownsWindow)
    {
        //
        // Bring the window on top of other ones (including the taskbar if it covers it completely)
        //
        // NOTE: To cover the taskbar with a window that does not completely cover it, the HWND_TOPMOST
        // Z-order must be used in the code below instead of HWND_TOP.
        // @todo: This should be controlled through a flag in the traits (topMostWindow)
        //

        if (!::SetWindowPos(_hwnd,
                            HWND_TOP,
                            _windowOriginXToRealize,
                            _windowOriginYToRealize,
                            _windowWidthToRealize,
                            _windowHeightToRealize,
                            SWP_SHOWWINDOW))
        {
            reportErrorForScreen("GraphicsWindowWin32::realizeImplementation() - Unable to show window", _traits->screenNum, ::GetLastError());
            return false;
        }
 
        if (!::UpdateWindow(_hwnd))
        {
            reportErrorForScreen("GraphicsWindowWin32::realizeImplementation() - Unable to update window", _traits->screenNum, ::GetLastError());
            return false;
        }
    }

    _realized = true;

    return true;
}

bool GraphicsWindowWin32::makeCurrentImplementation()
{
    if (!_realized)
    {
        reportErrorForScreen("GraphicsWindowWin32::makeCurrentImplementation() - Window not realized; cannot do makeCurrent.", _traits->screenNum, 0);
        return false;
    }

    if (!::wglMakeCurrent(_hdc, _hglrc))
    {
        reportErrorForScreen("GraphicsWindowWin32::makeCurrentImplementation() - Unable to set current OpenGL rendering context", _traits->screenNum, ::GetLastError());
        return false;
    }

    return true;
}

bool GraphicsWindowWin32::releaseContextImplementation()
{
    if (!::wglMakeCurrent(_hdc, NULL))
    {
        reportErrorForScreen("GraphicsWindowWin32::releaseContextImplementation() - Unable to release current OpenGL rendering context", _traits->screenNum, ::GetLastError());
        return false;
    }

    return true;
}

void GraphicsWindowWin32::closeImplementation()
{
    destroyWindow();

    _initialized = false;
    _valid       = false;
    _realized    = false;
}

void GraphicsWindowWin32::swapBuffersImplementation()
{
    if (!_realized) return;
    if (!::SwapBuffers(_hdc))
    {
        reportErrorForScreen("GraphicsWindowWin32::swapBuffersImplementation() - Unable to swap display buffers", _traits->screenNum, ::GetLastError());
    }
}

void GraphicsWindowWin32::checkEvents()
{
    if (!_realized) return;

    MSG msg;
    while (::PeekMessage(&msg, _hwnd, NULL, NULL, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    if (_closeWindow)
    {
        _closeWindow = false;
        close();
    }

    if (_destroyWindow)
    {
        _destroyWindow = false;
        destroyWindow(false);
    }
}

void GraphicsWindowWin32::grabFocus()
{
    if (!::SetForegroundWindow(_hwnd))
    {
        osg::notify(osg::WARN) << "Warning: GraphicsWindowWin32::grabFocus() - Failed grabbing the focus" << std::endl;
    }
}

void GraphicsWindowWin32::grabFocusIfPointerInWindow()
{
    POINT mousePos;
    if (!::GetCursorPos(&mousePos))
    {
        reportErrorForScreen("GraphicsWindowWin32::grabFocusIfPointerInWindow() - Unable to get cursor position", _traits->screenNum, ::GetLastError());
        return;
    }

    RECT windowRect;
    if (!::GetWindowRect(_hwnd, &windowRect))
    {
        reportErrorForScreen("GraphicsWindowWin32::grabFocusIfPointerInWindow() - Unable to get window position", _traits->screenNum, ::GetLastError());
        return;
    }

    if (mousePos.x>=windowRect.left && mousePos.x<=windowRect.right &&
        mousePos.y>=windowRect.top  && mousePos.y<=windowRect.bottom)
    {
        grabFocus();
    }
}

void GraphicsWindowWin32::requestWarpPointer( float x, float y )
{
    if (!_realized)
    {
        reportErrorForScreen("GraphicsWindowWin32::requestWarpPointer() - Window not realized; cannot warp pointer", _traits->screenNum, 0);
        return;
    }

    RECT windowRect;

    if (!::GetWindowRect(_hwnd, &windowRect))
    {
        reportErrorForScreen("GraphicsWindowWin32::requestWarpPointer() - Unable to get window rectangle", _traits->screenNum, ::GetLastError());
        return;
    }

    if (!::SetCursorPos(windowRect.left + x, windowRect.top + y))
    {
        reportErrorForScreen("GraphicsWindowWin32::requestWarpPointer() - Unable to set cursor position", _traits->screenNum, ::GetLastError());
        return;
    }
   
    getEventQueue()->mouseWarped(x,y);
}

void GraphicsWindowWin32::setWindowRectangle(int x, int y, int width, int height)
{
    if (!::SetWindowPos(_hwnd, HWND_TOP, x, y, width, height, SWP_SHOWWINDOW | SWP_FRAMECHANGED))
    {
        reportErrorForScreen("GraphicsWindowWin32::setWindowRectangle() - Unable to set new window position and size", _traits->screenNum, ::GetLastError());
    }
}

void GraphicsWindowWin32::useCursor( bool cursorOn )
{
    _traits->useCursor = cursorOn;
}

void GraphicsWindowWin32::adaptKey( WPARAM wParam, LPARAM lParam, int& keySymbol, unsigned int& modifierMask )
{
    modifierMask = 0;

    bool rightSide = (lParam & 0x01000000)!=0;
    int virtualKey = ::MapVirtualKeyEx((lParam>>16) & 0xff, 3, ::GetKeyboardLayout(0));

    BYTE keyState[256];

    if (virtualKey==0 || !::GetKeyboardState(keyState))
    {
        keySymbol = 0;
        return;
    }

    switch (virtualKey)
    {
        //////////////////
        case VK_LSHIFT   :
        //////////////////

        modifierMask |= osgGA::GUIEventAdapter::MODKEY_LEFT_SHIFT;
            break;

        //////////////////
        case VK_RSHIFT   :
        //////////////////

        modifierMask |= osgGA::GUIEventAdapter::MODKEY_RIGHT_SHIFT;
            break;

        //////////////////
        case VK_CONTROL  :
        case VK_LCONTROL :
        //////////////////

            virtualKey    = rightSide ? VK_RCONTROL : VK_LCONTROL;
            modifierMask |= rightSide ? osgGA::GUIEventAdapter::MODKEY_RIGHT_CTRL : osgGA::GUIEventAdapter::MODKEY_LEFT_CTRL;
            break;

        //////////////////
        case VK_MENU     :
        case VK_LMENU    :
        //////////////////

            virtualKey    = rightSide ? VK_RMENU : VK_LMENU;
            modifierMask |= rightSide ? osgGA::GUIEventAdapter::MODKEY_RIGHT_ALT : osgGA::GUIEventAdapter::MODKEY_LEFT_ALT;
            break;

        //////////////////
        default          :
        //////////////////

            virtualKey = wParam;
            break;
    }

    if (keyState[VK_CAPITAL] & 0x01) modifierMask |= osgGA::GUIEventAdapter::MODKEY_CAPS_LOCK;
    if (keyState[VK_NUMLOCK] & 0x01) modifierMask |= osgGA::GUIEventAdapter::MODKEY_NUM_LOCK;

    keySymbol = remapWin32Key(virtualKey);

    if (keySymbol==osgGA::GUIEventAdapter::KEY_Return && rightSide)
    {
        keySymbol = osgGA::GUIEventAdapter::KEY_KP_Enter;
    }
    else if ((keySymbol & 0xff00)==0) 
    {
        char asciiKey[2];
        int numChars = ::ToAscii(wParam, (lParam>>16)&0xff, keyState, reinterpret_cast<WORD*>(asciiKey), 0);
        if (numChars>0) keySymbol = asciiKey[0];
    }
}

void GraphicsWindowWin32::transformMouseXY( float& x, float& y )
{
    if (getEventQueue()->getUseFixedMouseInputRange())
    {
        osgGA::GUIEventAdapter* eventState = getEventQueue()->getCurrentEventState();

        x = eventState->getXmin() + (eventState->getXmax()-eventState->getXmin())*x/float(_traits->width);
        y = eventState->getYmin() + (eventState->getYmax()-eventState->getYmin())*y/float(_traits->height);
    }
}

LRESULT GraphicsWindowWin32::handleNativeWindowingEvent( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    //!@todo adapt windows event time to osgGA event queue time for better resolution

    double baseTime   = _timeOfLastCheckEvents;
    double eventTime  = _timeOfLastCheckEvents;
    double resizeTime = eventTime;

    _timeOfLastCheckEvents = getEventQueue()->getTime();

    switch(uMsg)
    {
        /////////////////
        case WM_PAINT   :
        /////////////////

            if (_ownsWindow)
            {
                PAINTSTRUCT paint;
                ::BeginPaint(hwnd, &paint);
                ::EndPaint(hwnd, &paint);
            }
            break;

        ///////////////////
        case WM_MOUSEMOVE :
        ///////////////////

            {
                float mx = GET_X_LPARAM(lParam);
                float my = GET_Y_LPARAM(lParam);
                transformMouseXY(mx, my);
                getEventQueue()->mouseMotion(mx, my, eventTime);
            }
            break;

        /////////////////////
        case WM_LBUTTONDOWN :
        case WM_MBUTTONDOWN :
        case WM_RBUTTONDOWN :
        /////////////////////

            {
                ::SetCapture(hwnd);

                int button;

                if (uMsg==WM_LBUTTONDOWN)      button = 1;
                else if (uMsg==WM_MBUTTONDOWN) button = 2;
                else button = 3;

                float mx = GET_X_LPARAM(lParam);
                float my = GET_Y_LPARAM(lParam);
                transformMouseXY(mx, my);
                getEventQueue()->mouseButtonPress(mx, my, button, eventTime);
            }
            break;

        /////////////////////
        case WM_LBUTTONUP   :
        case WM_MBUTTONUP   :
        case WM_RBUTTONUP   :
        /////////////////////
            
            {
                ::ReleaseCapture();

                int button;

                if (uMsg==WM_LBUTTONUP)      button = 1;
                else if (uMsg==WM_MBUTTONUP) button = 2;
                else button = 3;

                float mx = GET_X_LPARAM(lParam);
                float my = GET_Y_LPARAM(lParam);
                transformMouseXY(mx, my);
                getEventQueue()->mouseButtonRelease(mx, my, button, eventTime);
            }
            break;

        ////////////////////
        case WM_MOUSEWHEEL :
        ////////////////////

            getEventQueue()->mouseScroll(GET_WHEEL_DELTA_WPARAM(wParam)<0 ? osgGA::GUIEventAdapter::SCROLL_DOWN :
                                                                            osgGA::GUIEventAdapter::SCROLL_UP,
                                         eventTime);
            break;

        /////////////////
        case WM_MOVE    :
        case WM_SIZE    :
        /////////////////

            {
                POINT origin;
                origin.x = 0;
                origin.y = 0;

                ::ClientToScreen(hwnd, &origin);

                int windowX = origin.x - _screenOriginX;
                int windowY = origin.y - _screenOriginY;
                resizeTime  = eventTime;

                RECT clientRect;
                ::GetClientRect(hwnd, &clientRect);

                int windowWidth;
                int windowHeight;

                if (clientRect.bottom==0 && clientRect.right==0)
                {
                    //
                    // Window has been minimized; keep window width & height to a minimum of 1 pixel
                    //

                    windowWidth  = 1;
                    windowHeight = 1;
                }
                else
                {
                    windowWidth  = clientRect.right;
                    windowHeight = clientRect.bottom;
                }

                if (windowX!=_traits->x || windowY!=_traits->y || windowWidth!=_traits->width || windowHeight!=_traits->height)
                {
                    resized(windowX, windowY, windowWidth, windowHeight); 
                    getEventQueue()->windowResize(windowX, windowY, windowWidth, windowHeight, resizeTime);
                }
            }
            break;

        ////////////////////
        case WM_KEYDOWN    :
        case WM_SYSKEYDOWN :
        ////////////////////

            {
                int keySymbol = 0;
                unsigned int modifierMask = 0;
                adaptKey(wParam, lParam, keySymbol, modifierMask);
                getEventQueue()->getCurrentEventState()->setModKeyMask(modifierMask);
                getEventQueue()->keyPress(keySymbol, eventTime);
            }
            break;

        //////////////////
        case WM_KEYUP    :
        case WM_SYSKEYUP :
        //////////////////

            {
                int keySymbol = 0;
                unsigned int modifierMask = 0;
                adaptKey(wParam, lParam, keySymbol, modifierMask);
                getEventQueue()->getCurrentEventState()->setModKeyMask(modifierMask);
                getEventQueue()->keyRelease(keySymbol, eventTime);
            }
            break;

        ///////////////////
        case WM_SETCURSOR :
        ///////////////////

            if (_traits->useCursor) return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
            ::SetCursor(NULL);
            return TRUE;

        /////////////////
        case WM_CLOSE   :
        /////////////////

            getEventQueue()->closeWindow(eventTime);
            break;

        /////////////////
        case WM_DESTROY :
        /////////////////

            _destroyWindow = true;
            if (_ownsWindow)
            {
                ::PostQuitMessage(0);
            }
            break;

        //////////////
        case WM_QUIT :
        //////////////

            _closeWindow = true;
            return wParam;

        /////////////////
        default         :
        /////////////////

            if (_ownsWindow) return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
            break;
    }

    if (_ownsWindow) return 0;

    return _windowProcedure==0 ? ::DefWindowProc(hwnd, uMsg, wParam, lParam) :
                                 ::CallWindowProc(_windowProcedure, hwnd, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////
//                  GraphicsContextWin32 implementation
//////////////////////////////////////////////////////////////////////////////

GraphicsContextWin32::GraphicsContextWin32( osg::GraphicsContext::Traits* traits )
: _valid(false)
{
    _traits = traits;
}

GraphicsContextWin32::~GraphicsContextWin32()
{
}
    
bool GraphicsContextWin32::valid() const
{
    return _valid;
}

bool GraphicsContextWin32::realizeImplementation()
{
    osg::notify(osg::NOTICE) << "GraphicsContextWin32::realizeImplementation() not implemented." << std::endl; return false;
}

bool GraphicsContextWin32::isRealizedImplementation() const
{
    osg::notify(osg::NOTICE) << "GraphicsContextWin32::isRealizedImplementation() not implemented." << std::endl; return false;
}

void GraphicsContextWin32::closeImplementation()
{
    osg::notify(osg::NOTICE) << "GraphicsContextWin32::closeImplementation() not implemented." << std::endl;
}

bool GraphicsContextWin32::makeCurrentImplementation()
{
    osg::notify(osg::NOTICE) << "GraphicsContextWin32::makeCurrentImplementation() not implemented." << std::endl;
    return false;
}
        
bool GraphicsContextWin32::makeContextCurrentImplementation( GraphicsContext* /*readContext*/ )
{
    osg::notify(osg::NOTICE) << "GraphicsContextWin32::makeContextCurrentImplementation(..) not implemented." << std::endl;
    return false;
}

bool GraphicsContextWin32::releaseContextImplementation()
{
    osg::notify(osg::NOTICE) << "GraphicsContextWin32::releaseContextImplementation(..) not implemented." << std::endl;
    return false;
}

void GraphicsContextWin32::bindPBufferToTextureImplementation( GLenum /*buffer*/ )
{
    osg::notify(osg::NOTICE) << "GraphicsContextWin32::void bindPBufferToTextureImplementation(..) not implemented." << std::endl;
}

void GraphicsContextWin32::swapBuffersImplementation() 
{
    osg::notify(osg::NOTICE) << "GraphicsContextWin32:: swapBuffersImplementation() not implemented." << std::endl;
}

//////////////////////////////////////////////////////////////////////////////
//  Class responsible for registering the Win32 Windowing System interface
//////////////////////////////////////////////////////////////////////////////

struct RegisterWindowingSystemInterfaceProxy
{
    RegisterWindowingSystemInterfaceProxy()
    {
        osg::GraphicsContext::setWindowingSystemInterface(Win32WindowingSystem::getInterface());
    }

    ~RegisterWindowingSystemInterfaceProxy()
    {
        osg::GraphicsContext::setWindowingSystemInterface(0);
    }
};

static RegisterWindowingSystemInterfaceProxy createWindowingSystemInterfaceProxy;

}; // namespace OsgViewer
