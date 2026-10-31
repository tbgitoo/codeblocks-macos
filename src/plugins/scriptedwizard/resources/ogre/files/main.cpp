#include <Ogre.h>
#include <OgreApplicationContext.h>

class MonsterRotator : public Ogre::FrameListener
{
public:
    explicit MonsterRotator(Ogre::SceneNode* node)
        : mNode(node)
    {
    }

    bool frameStarted(const Ogre::FrameEvent& evt) override
    {
        mNode->yaw(
            Ogre::Degree(20.0f * evt.timeSinceLastFrame),
            Ogre::Node::TS_PARENT);

        return true;
    }

private:
    Ogre::SceneNode* mNode;
};

int main()
{
    OgreBites::ApplicationContext app("OgreM1Probe");
    app.initApp();

    Ogre::Root* root = app.getRoot();

    Ogre::SceneManager* sceneManager =
        root->createSceneManager();

    sceneManager->setAmbientLight(
        Ogre::ColourValue(0.35f, 0.35f, 0.35f));

    Ogre::Light* light =
        sceneManager->createLight("MainLight");

    Ogre::SceneNode* lightNode =
        sceneManager->getRootSceneNode()
                    ->createChildSceneNode();

    lightNode->setPosition(20.0f, 80.0f, 100.0f);
    lightNode->attachObject(light);

    Ogre::Entity* monster =
        sceneManager->createEntity("ogrehead.mesh");

    Ogre::SceneNode* monsterNode =
        sceneManager->getRootSceneNode()
                    ->createChildSceneNode();

    monsterNode->attachObject(monster);

    MonsterRotator rotator(monsterNode);
    root->addFrameListener(&rotator);

    Ogre::Camera* camera =
        sceneManager->createCamera("MainCamera");

    camera->setNearClipDistance(0.1f);
    camera->setAutoAspectRatio(true);

    Ogre::SceneNode* cameraNode =
        sceneManager->getRootSceneNode()
                    ->createChildSceneNode();

    cameraNode->setPosition(0.0f, 0.0f, 150.0f);

    cameraNode->lookAt(
        Ogre::Vector3(0.0f, 0.0f, 0.0f),
        Ogre::Node::TS_PARENT);

    cameraNode->attachObject(camera);

    Ogre::Viewport* viewport =
        app.getRenderWindow()->addViewport(camera);

    viewport->setBackgroundColour(
        Ogre::ColourValue(0.08f, 0.12f, 0.20f));

    root->startRendering();

    app.closeApp();
    return 0;
}
