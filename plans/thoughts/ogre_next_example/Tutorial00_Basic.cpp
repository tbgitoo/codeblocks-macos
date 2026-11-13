//---------------------------------------------------------------------------------------
// Tutorial00_Basic.cpp
//
// Independent consumer of the installed OGRE-Next SDK.
// Visual demonstration: rotating Ogre head with three orbiting coloured lights.
//
// Required media beside the installed SDK:
//   ../../Media/models/ogrehead.mesh
// when this executable is run from install/bin/Release.
//---------------------------------------------------------------------------------------

// This example expects ogrehead.mesh in the current working directory.
// See LICENSE-OGRE.txt for the applicable license and copyright notice.

#include "OgreAbiUtils.h"
#include "OgreArchiveManager.h"
#include "OgreCamera.h"
#include "OgreConfigFile.h"
#include "OgreException.h"
#include "OgreItem.h"
#include "OgreLight.h"
#include "OgreMeshManager.h"
#include "OgreMeshManager2.h"
#include "OgreResourceGroupManager.h"
#include "OgreRoot.h"
#include "OgreSceneManager.h"
#include "OgreSceneNode.h"
#include "OgreWindow.h"

#include "OgreHlmsManager.h"
#include "OgreHlmsPbs.h"
#include "OgreHlmsPbsDatablock.h"
#include "OgreHlmsUnlit.h"
#include "OgreHlmsUnlitDatablock.h"

#include "Compositor/OgreCompositorManager2.h"
#include "OgreWindowEventUtilities.h"

#include <chrono>
#include <cmath>
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
                archiveManager.load( rootHlmsFolder + *libraryFolderPathIt,
                                     "FileSystem", true );
            archiveUnlitLibraryFolders.push_back( archiveLibrary );
            ++libraryFolderPathIt;
        }

        hlmsUnlit = OGRE_NEW HlmsUnlit( archiveUnlit, &archiveUnlitLibraryFolders );
        Root::getSingleton().getHlmsManager()->registerHlms( hlmsUnlit );
    }

    {
        HlmsPbs::getDefaultPaths( mainFolderPath, libraryFoldersPaths );
        Archive *archivePbs =
            archiveManager.load( rootHlmsFolder + mainFolderPath, "FileSystem", true );

        ArchiveVec archivePbsLibraryFolders;
        libraryFolderPathIt = libraryFoldersPaths.begin();
        libraryFolderPathEn = libraryFoldersPaths.end();
        while( libraryFolderPathIt != libraryFolderPathEn )
        {
            Archive *archiveLibrary =
                archiveManager.load( rootHlmsFolder + *libraryFolderPathIt,
                                     "FileSystem", true );
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
    void windowClosed( Ogre::Window *rw ) override
    {
        (void)rw;
        mQuit = true;
    }

    bool getQuit() const { return mQuit; }
};

static Ogre::RenderSystem *selectRenderSystem( Ogre::Root *root )
{
    const char *preferred[] = {
        "Direct3D11 Rendering Subsystem",
        "OpenGL 3+ Rendering Subsystem",
        "Vulkan Rendering Subsystem"
    };

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

static Ogre::HlmsPbsDatablock *createPbsDatablock( const char *name )
{
    using namespace Ogre;

    HlmsPbs *hlmsPbs = static_cast<HlmsPbs *>(
        Root::getSingleton().getHlmsManager()->getHlms( HLMS_PBS ) );
    return static_cast<HlmsPbsDatablock *>( hlmsPbs->createDatablock(
        name, name, HlmsMacroblock(), HlmsBlendblock(), HlmsParamVec() ) );
}

// ogrehead.mesh names v1 materials. Items resolve those names as HLMS
// datablocks, so define them before the mesh is instantiated.
static void registerOgreHeadMaterials()
{
    using namespace Ogre;

    HlmsPbsDatablock *skin = createPbsDatablock( "Ogre/Skin" );
    skin->setTexture( PBSM_DIFFUSE, "GreenSkin.jpg" );
    skin->setRoughness( 0.8f );
    skin->setFresnel( Vector3( 0.03f ), false );

    HlmsPbsDatablock *eyes = createPbsDatablock( "Ogre/Eyes" );
    eyes->setDiffuse( Vector3( 0.9f, 0.95f, 0.85f ) );
    eyes->setRoughness( 0.12f );
    eyes->setFresnel( Vector3( 0.05f ), false );

    HlmsPbsDatablock *earring = createPbsDatablock( "Ogre/Earring" );
    earring->setDiffuse( Vector3( 1.0f, 0.72f, 0.18f ) );
    earring->setMetalness( 1.0f );
    earring->setRoughness( 0.22f );

    HlmsPbsDatablock *tusks = createPbsDatablock( "Ogre/Tusks" );
    tusks->setTexture( PBSM_DIFFUSE, "tusk.jpg" );
    tusks->setRoughness( 0.45f );
    tusks->setFresnel( Vector3( 0.04f ), false );
}

static Ogre::Item *createOgreHead( Ogre::SceneManager *sceneManager )
{
    using namespace Ogre;

    const String group = ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;

    // ogrehead.mesh is an OGRE v1 mesh. Load it through the v1 manager and
    // import it into OGRE-Next's v2 mesh representation.
    v1::MeshPtr meshV1 = v1::MeshManager::getSingleton().load(
        "ogrehead.mesh", group,
        v1::HardwareBuffer::HBU_STATIC,
        v1::HardwareBuffer::HBU_STATIC );

    MeshPtr meshV2 = MeshManager::getSingleton().createByImportingV1(
        "ogrehead-v2.mesh", group, meshV1.get(),
        true, true, true );

    return sceneManager->createItem( meshV2, SCENE_DYNAMIC );
}

// Prefab_Sphere has radius 50. This scale makes an orb about 0.4 units across.
static const Ogre::Real kLightBallScale = 0.008f;

static Ogre::MeshPtr loadLightSphere()
{
    using namespace Ogre;

    v1::MeshPtr sphereV1 = v1::MeshManager::getSingleton().getByName(
        "Prefab_Sphere", ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME );

    return MeshManager::getSingleton().createByImportingV1(
        "light-sphere.mesh", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        sphereV1.get(), true, true, false );
}

static void attachLightBall( Ogre::SceneManager *sceneManager, Ogre::SceneNode *lightNode,
                             const Ogre::MeshPtr &sphereMesh, const char *datablockName,
                             const Ogre::ColourValue &colour )
{
    using namespace Ogre;

    HlmsUnlit *hlmsUnlit = static_cast<HlmsUnlit *>(
        Root::getSingleton().getHlmsManager()->getHlms( HLMS_UNLIT ) );
    HlmsUnlitDatablock *datablock = static_cast<HlmsUnlitDatablock *>(
        hlmsUnlit->createDatablock( datablockName, datablockName, HlmsMacroblock(),
                                    HlmsBlendblock(), HlmsParamVec() ) );
    datablock->setUseColour( true );
    datablock->setColour( colour );

    Item *ball = sceneManager->createItem( sphereMesh, SCENE_DYNAMIC );
    ball->setDatablock( datablock );

    SceneNode *ballNode = lightNode->createChildSceneNode( SCENE_DYNAMIC );
    ballNode->setScale( kLightBallScale, kLightBallScale, kLightBallScale );
    ballNode->attachObject( ball );
}

int main( int argc, const char *argv[] )
{
    using namespace Ogre;
    (void)argc;
    (void)argv;

    const String pluginsFolder = "./";
    const String writeAccessFolder = pluginsFolder;
    const char *pluginsFile = "plugins.cfg";

    const AbiCookie abiCookie = generateAbiCookie();
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
        renderSystem->setConfigOption( "Full Screen", "No" );
        renderSystem->setConfigOption( "Video Mode", "1280 x 720 @ 32-bit colour" );
        renderSystem->setConfigOption( "sRGB Gamma Conversion", "Yes" );

        Window *window = root->initialise( true, "OGRE-Next Light Show" );

        registerHlms();

        ResourceGroupManager &resourceManager = ResourceGroupManager::getSingleton();
        const String group = ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;
        resourceManager.addResourceLocation( ".", "FileSystem", group, false );
        // Textures shipped with the installed SDK. This path is relative to
        // install/bin/Release, which is the working directory this sample expects.
        resourceManager.addResourceLocation(
            "../../Media/materials/textures", "FileSystem", group, false );
        resourceManager.initialiseAllResourceGroups( true );
        registerOgreHeadMaterials();

        const size_t numThreads = 1u;
        SceneManager *sceneManager =
            root->createSceneManager( ST_GENERIC, numThreads, "LightShowScene" );

        // Directional lights are baked into the PBS shader. Point lights are
        // only applied when clustered forward lighting is enabled.
        sceneManager->setForwardClustered( true, 16, 8, 24, 96, 0, 0, 1.0f, 200.0f );

        sceneManager->setAmbientLight(
            ColourValue( 0.03f, 0.03f, 0.05f ),
            ColourValue( 0.03f, 0.03f, 0.05f ),
            Vector3::UNIT_Y );

        Item *ogreHead = createOgreHead( sceneManager );
        SceneNode *ogreHeadNode =
            sceneManager->getRootSceneNode( SCENE_DYNAMIC )
                ->createChildSceneNode( SCENE_DYNAMIC );
        ogreHeadNode->attachObject( ogreHead );
        ogreHeadNode->setPosition( 0.0f, 0.0f, 0.0f );
        ogreHeadNode->setScale( 0.12f, 0.12f, 0.12f );

        // setDirection writes the parent SceneNode. Attach first.
        Light *fillLight = sceneManager->createLight();
        SceneNode *fillLightNode =
            sceneManager->getRootSceneNode( SCENE_DYNAMIC )
                ->createChildSceneNode( SCENE_DYNAMIC );
        fillLightNode->attachObject( fillLight );
        fillLight->setType( Light::LT_DIRECTIONAL );
        fillLight->setDirection( Vector3( -1.0f, -1.0f, -1.0f ).normalisedCopy() );
        fillLight->setDiffuseColour( 0.14f, 0.15f, 0.18f );
        fillLight->setSpecularColour( 0.08f, 0.09f, 0.11f );
        fillLight->setPowerScale( 1.0f );

        const MeshPtr lightSphere = loadLightSphere();
        const ColourValue redColour( 1.0f, 0.05f, 0.02f );
        const ColourValue blueColour( 0.05f, 0.25f, 1.0f );
        const ColourValue goldColour( 1.0f, 0.62f, 0.12f );

        // Linear brightness of each point light. The shader multiplies this
        // into the colour, then divides by (0.5 + quadratic * distance^2).
        // This window does not tonemap, so values much above 1 clip to the
        // same saturated colour. The radius only changes that distance falloff.
        const Real pointLightPower = 0.5f;

        Light *redLight = sceneManager->createLight();
        redLight->setType( Light::LT_POINT );
        redLight->setDiffuseColour( redColour );
        redLight->setSpecularColour( redColour );
        redLight->setPowerScale( pointLightPower );
        redLight->setAttenuationBasedOnRadius( 8.0f, 0.05f );

        SceneNode *redLightNode =
            sceneManager->getRootSceneNode( SCENE_DYNAMIC )
                ->createChildSceneNode( SCENE_DYNAMIC );
        redLightNode->attachObject( redLight );
        attachLightBall( sceneManager, redLightNode, lightSphere, "LightBall/Red", redColour );

        Light *blueLight = sceneManager->createLight();
        blueLight->setType( Light::LT_POINT );
        blueLight->setDiffuseColour( blueColour );
        blueLight->setSpecularColour( blueColour );
        blueLight->setPowerScale( pointLightPower );
        blueLight->setAttenuationBasedOnRadius( 8.0f, 0.05f );

        SceneNode *blueLightNode =
            sceneManager->getRootSceneNode( SCENE_DYNAMIC )
                ->createChildSceneNode( SCENE_DYNAMIC );
        blueLightNode->attachObject( blueLight );
        attachLightBall( sceneManager, blueLightNode, lightSphere, "LightBall/Blue", blueColour );

        Light *goldLight = sceneManager->createLight();
        goldLight->setType( Light::LT_POINT );
        goldLight->setDiffuseColour( goldColour );
        goldLight->setSpecularColour( goldColour );
        goldLight->setPowerScale( pointLightPower );
        goldLight->setAttenuationBasedOnRadius( 8.0f, 0.05f );

        SceneNode *goldLightNode =
            sceneManager->getRootSceneNode( SCENE_DYNAMIC )
                ->createChildSceneNode( SCENE_DYNAMIC );
        goldLightNode->attachObject( goldLight );
        attachLightBall( sceneManager, goldLightNode, lightSphere, "LightBall/Gold", goldColour );

        Camera *camera = sceneManager->createCamera( "Main Camera" );
        camera->setPosition( Vector3( 0.0f, 2.4f, 16.0f ) );
        camera->lookAt( Vector3( 0.0f, 1.0f, 0.0f ) );
        camera->setNearClipDistance( 0.2f );
        camera->setFarClipDistance( 1000.0f );
        camera->setAutoAspectRatio( true );

        CompositorManager2 *compositorManager = root->getCompositorManager2();
        const String workspaceName( "Light Show Workspace" );
        const ColourValue backgroundColour( 0.003f, 0.006f, 0.020f );
        compositorManager->createBasicWorkspaceDef(
            workspaceName, backgroundColour, IdString() );
        compositorManager->addWorkspace(
            sceneManager, window->getTexture(), camera,
            workspaceName, true );

        MyWindowEventListener windowListener;
        WindowEventUtilities::addWindowEventListener( window, &windowListener );

        typedef std::chrono::steady_clock Clock;
        const Clock::time_point startTime = Clock::now();
        Clock::time_point previousTime = startTime;

        bool quit = false;
        while( !quit )
        {
            WindowEventUtilities::messagePump();
            quit |= windowListener.getQuit();

            const Clock::time_point currentTime = Clock::now();
            const float deltaTime =
                std::chrono::duration<float>( currentTime - previousTime ).count();
            const float elapsedTime =
                std::chrono::duration<float>( currentTime - startTime ).count();
            previousTime = currentTime;

            ogreHeadNode->yaw( Radian( deltaTime * 0.45f ) );
            ogreHeadNode->pitch( Radian( std::sin( elapsedTime * 0.7f ) *
                                         deltaTime * 0.12f ) );

            const float redAngle = elapsedTime * 0.85f;
            const float blueAngle = elapsedTime * -0.58f;
            const float goldAngle = elapsedTime * 0.37f;

            redLightNode->setPosition(
                std::cos( redAngle ) * 6.5f,
                1.6f + std::sin( redAngle * 1.7f ) * 1.6f,
                std::sin( redAngle ) * 6.5f );

            blueLightNode->setPosition(
                std::cos( blueAngle ) * 8.0f,
                1.2f + std::sin( blueAngle * 1.3f ) * 2.4f,
                std::sin( blueAngle ) * 8.0f );

            goldLightNode->setPosition(
                std::cos( goldAngle ) * 5.2f,
                3.4f + std::sin( goldAngle * 2.1f ) * 1.0f,
                std::sin( goldAngle ) * 5.2f );

            if( !quit )
                quit |= !root->renderOneFrame();
        }

        WindowEventUtilities::removeWindowEventListener( window, &windowListener );
        std::printf( "Light show rendered and closed cleanly.\n" );
    }
    catch( const Exception &e )
    {
        std::fprintf( stderr, "OGRE exception: %s\n",
                      e.getFullDescription().c_str() );
        OGRE_DELETE root;
        return 1;
    }

    OGRE_DELETE root;
    return 0;
}
