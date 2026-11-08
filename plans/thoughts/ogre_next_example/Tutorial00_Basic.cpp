//---------------------------------------------------------------------------------------
// Independent consumer of the installed OGRE-Next SDK.
// Initialization follows Samples/2.0/Tutorials/Tutorial00_Basic.
// The config dialog is replaced by an explicit render-system choice so the
// program can open a window without a mouse click.
//---------------------------------------------------------------------------------------

#include "OgreAbiUtils.h"
#include "OgreArchiveManager.h"
#include "OgreCamera.h"
#include "OgreConfigFile.h"
#include "OgreException.h"
#include "OgreRoot.h"
#include "OgreWindow.h"

#include "OgreHlmsManager.h"
#include "OgreHlmsPbs.h"
#include "OgreHlmsUnlit.h"

#include "Compositor/OgreCompositorManager2.h"

#include "OgreWindowEventUtilities.h"

#include <cstdio>

static void registerHlms()
{
    using namespace Ogre;

    String resourcePath = "";

    ConfigFile cf;
    cf.load( resourcePath + "resources2.cfg" );

    String rootHlmsFolder = resourcePath + cf.getSetting( "DoNotUseAsResource", "Hlms", "" );

    if( rootHlmsFolder.empty() )
        rootHlmsFolder = "./";
    else if( *( rootHlmsFolder.end() - 1 ) != '/' )
        rootHlmsFolder += "/";

    HlmsUnlit *hlmsUnlit = 0;
    HlmsPbs *hlmsPbs = 0;

    String mainFolderPath;
    StringVector libraryFoldersPaths;
    StringVector::const_iterator libraryFolderPathIt;
    StringVector::const_iterator libraryFolderPathEn;

    ArchiveManager &archiveManager = ArchiveManager::getSingleton();

    {
        HlmsUnlit::getDefaultPaths( mainFolderPath, libraryFoldersPaths );
        Archive *archiveUnlit =
            archiveManager.load( rootHlmsFolder + mainFolderPath, "FileSystem", true );
        ArchiveVec archiveUnlitLibraryFolders;
        libraryFolderPathIt = libraryFoldersPaths.begin();
        libraryFolderPathEn = libraryFoldersPaths.end();
        while( libraryFolderPathIt != libraryFolderPathEn )
        {
            Archive *archiveLibrary =
                archiveManager.load( rootHlmsFolder + *libraryFolderPathIt, "FileSystem", true );
            archiveUnlitLibraryFolders.push_back( archiveLibrary );
            ++libraryFolderPathIt;
        }

        hlmsUnlit = OGRE_NEW HlmsUnlit( archiveUnlit, &archiveUnlitLibraryFolders );
        Root::getSingleton().getHlmsManager()->registerHlms( hlmsUnlit );
    }

    {
        HlmsPbs::getDefaultPaths( mainFolderPath, libraryFoldersPaths );
        Archive *archivePbs = archiveManager.load( rootHlmsFolder + mainFolderPath, "FileSystem", true );

        ArchiveVec archivePbsLibraryFolders;
        libraryFolderPathIt = libraryFoldersPaths.begin();
        libraryFolderPathEn = libraryFoldersPaths.end();
        while( libraryFolderPathIt != libraryFolderPathEn )
        {
            Archive *archiveLibrary =
                archiveManager.load( rootHlmsFolder + *libraryFolderPathIt, "FileSystem", true );
            archivePbsLibraryFolders.push_back( archiveLibrary );
            ++libraryFolderPathIt;
        }

        hlmsPbs = OGRE_NEW HlmsPbs( archivePbs, &archivePbsLibraryFolders );
        Root::getSingleton().getHlmsManager()->registerHlms( hlmsPbs );
    }

    RenderSystem *renderSystem = Root::getSingletonPtr()->getRenderSystem();
    if( renderSystem->getName() == "Direct3D11 Rendering Subsystem" )
    {
        bool supportsNoOverwriteOnTextureBuffers;
        renderSystem->getCustomAttribute( "MapNoOverwriteOnDynamicBufferSRV",
                                           &supportsNoOverwriteOnTextureBuffers );

        if( !supportsNoOverwriteOnTextureBuffers )
        {
            hlmsPbs->setTextureBufferDefaultSize( 512 * 1024 );
            hlmsUnlit->setTextureBufferDefaultSize( 512 * 1024 );
        }
    }
}

class MyWindowEventListener final : public Ogre::WindowEventListener
{
    bool mQuit;

public:
    MyWindowEventListener() : mQuit( false ) {}
    void windowClosed( Ogre::Window *rw ) override { mQuit = true; }

    bool getQuit() const { return mQuit; }
};

static Ogre::RenderSystem *selectRenderSystem( Ogre::Root *root )
{
    const char *preferred[] = { "Direct3D11 Rendering Subsystem",
                                "OpenGL 3+ Rendering Subsystem",
                                "Vulkan Rendering Subsystem" };
    for( const char *name : preferred )
    {
        Ogre::RenderSystem *renderSystem = root->getRenderSystemByName( name );
        if( renderSystem )
        {
            std::printf( "Selected render system: %s\n", name );
            return renderSystem;
        }
    }
    return 0;
}

int main( int argc, const char *argv[] )
{
    using namespace Ogre;
    (void)argc;
    (void)argv;

    const String pluginsFolder = "./";
    const String writeAccessFolder = pluginsFolder;
    const char *pluginsFile = "plugins.cfg";

    const Ogre::AbiCookie abiCookie = Ogre::generateAbiCookie();
    Root *root = OGRE_NEW Root( &abiCookie, pluginsFolder + pluginsFile,
                                writeAccessFolder + "ogre.cfg",
                                writeAccessFolder + "Ogre.log" );

    try
    {
        RenderSystem *renderSystem = selectRenderSystem( root );
        if( !renderSystem )
        {
            std::fprintf( stderr, "No render system was loaded from plugins.cfg\n" );
            OGRE_DELETE root;
            return 1;
        }

        root->setRenderSystem( renderSystem );
        // D3D11 defaults to exclusive fullscreen and the last listed mode.
        // A fixed windowed mode matches the window a student can see and close.
        renderSystem->setConfigOption( "Full Screen", "No" );
        renderSystem->setConfigOption( "Video Mode", "1280 x 720 @ 32-bit colour" );
        renderSystem->setConfigOption( "sRGB Gamma Conversion", "Yes" );
        Window *window = root->initialise( true, "Tutorial 00: Basic" );

        registerHlms();

        const size_t numThreads = 1u;
        SceneManager *sceneManager = root->createSceneManager( ST_GENERIC, numThreads, "ExampleSMInstance" );

        Camera *camera = sceneManager->createCamera( "Main Camera" );
        camera->setPosition( Vector3( 0, 5, 15 ) );
        camera->lookAt( Vector3( 0, 0, 0 ) );
        camera->setNearClipDistance( 0.2f );
        camera->setFarClipDistance( 1000.0f );
        camera->setAutoAspectRatio( true );

        CompositorManager2 *compositorManager = root->getCompositorManager2();
        const String workspaceName( "Demo Workspace" );
        const ColourValue backgroundColour( 0.2f, 0.4f, 0.6f );
        compositorManager->createBasicWorkspaceDef( workspaceName, backgroundColour, IdString() );
        compositorManager->addWorkspace( sceneManager, window->getTexture(), camera, workspaceName, true );

        MyWindowEventListener myWindowEventListener;
        WindowEventUtilities::addWindowEventListener( window, &myWindowEventListener );

        bool bQuit = false;
        while( !bQuit )
        {
            WindowEventUtilities::messagePump();
            bQuit |= myWindowEventListener.getQuit();
            if( !bQuit )
                bQuit |= !root->renderOneFrame();
        }

        WindowEventUtilities::removeWindowEventListener( window, &myWindowEventListener );
        std::printf( "Rendered and closed cleanly.\n" );
    }
    catch( const Exception &e )
    {
        std::fprintf( stderr, "OGRE exception: %s\n", e.getFullDescription().c_str() );
        OGRE_DELETE root;
        return 1;
    }

    OGRE_DELETE root;
    return 0;
}
