#include <algorithm>
#include <vector>

#include <Ogre.h>
#include <OgreApplicationContext.h>

struct OrbitExtent
{
    Ogre::Real radial;
    Ogre::Real absY;
};

void appendExtents(
    const Ogre::VertexData* vertexData,
    const Ogre::Vector3& center,
    std::vector<OrbitExtent>& extents)
{
    if (!vertexData)
        return;

    const Ogre::VertexElement* position =
        vertexData->vertexDeclaration->findElementBySemantic(
            Ogre::VES_POSITION);

    if (!position || position->getType() != Ogre::VET_FLOAT3)
        return;

    Ogre::HardwareVertexBufferSharedPtr buffer =
        vertexData->vertexBufferBinding->getBuffer(position->getSource());

    unsigned char* data = static_cast<unsigned char*>(
        buffer->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));

    const size_t stride = buffer->getVertexSize();

    for (size_t index = 0; index < vertexData->vertexCount; ++index)
    {
        float* coordinates = nullptr;
        position->baseVertexPointerToElement(data, &coordinates);

        const Ogre::Real x = coordinates[0] - center.x;
        const Ogre::Real y = coordinates[1] - center.y;
        const Ogre::Real z = coordinates[2] - center.z;

        OrbitExtent extent;
        extent.radial = Ogre::Math::Sqrt(x * x + z * z);
        extent.absY = Ogre::Math::Abs(y);
        extents.push_back(extent);

        data += stride;
    }

    buffer->unlock();
}

std::vector<OrbitExtent> meshExtents(
    Ogre::Entity* entity,
    const Ogre::Vector3& center)
{
    std::vector<OrbitExtent> extents;
    const Ogre::MeshPtr mesh = entity->getMesh();

    try
    {
        appendExtents(mesh->sharedVertexData, center, extents);

        for (unsigned short index = 0;
             index < mesh->getNumSubMeshes();
             ++index)
        {
            const Ogre::SubMesh* subMesh = mesh->getSubMesh(index);

            if (!subMesh->useSharedVertices)
                appendExtents(subMesh->vertexData, center, extents);
        }
    }
    catch (const Ogre::Exception&)
    {
        extents.clear();
    }

    return extents;
}

Ogre::Real viewingDistance(
    const std::vector<OrbitExtent>& extents,
    const Ogre::AxisAlignedBox& bounds,
    Ogre::Real aspect,
    Ogre::Radian fovY)
{
    const Ogre::Real margin = static_cast<Ogre::Real>(1.08);
    const Ogre::Real tanY = Ogre::Math::Tan(fovY * static_cast<Ogre::Real>(0.5));

    if (tanY <= 0.0f)
        return 150.0f;

    if (aspect < static_cast<Ogre::Real>(0.1))
        aspect = static_cast<Ogre::Real>(4.0 / 3.0);

    const Ogre::Real tanX = tanY * aspect;
    const Ogre::Real sinX = tanX / Ogre::Math::Sqrt(1.0f + tanX * tanX);
    const Ogre::Real sinY = tanY / Ogre::Math::Sqrt(1.0f + tanY * tanY);

    Ogre::Real distance = 0.0f;

    for (const OrbitExtent& extent : extents)
    {
        const Ogre::Real horizontal = extent.radial / sinX;
        const Ogre::Real vertical =
            extent.radial + extent.absY / tanY;

        distance = std::max(distance, std::max(horizontal, vertical));
    }

    if (distance <= 0.0f)
    {
        const Ogre::Real radius = bounds.getHalfSize().length();
        distance = radius / std::min(sinX, sinY);
    }

    if (distance <= 0.0f)
        return 150.0f;

    return distance * margin;
}

class SpinningMeshView : public Ogre::FrameListener
{
public:
    SpinningMeshView(
        Ogre::SceneNode* meshNode,
        Ogre::SceneNode* cameraNode,
        Ogre::Camera* camera,
        Ogre::Viewport* viewport,
        const Ogre::AxisAlignedBox& bounds,
        std::vector<OrbitExtent> extents)
        : mMeshNode(meshNode)
        , mCameraNode(cameraNode)
        , mCamera(camera)
        , mViewport(viewport)
        , mBounds(bounds)
        , mExtents(std::move(extents))
    {
    }

    void updateCamera()
    {
        Ogre::Real aspect = mCamera->getAspectRatio();

        if (mViewport->getActualWidth() > 0 &&
            mViewport->getActualHeight() > 0)
        {
            aspect =
                static_cast<Ogre::Real>(mViewport->getActualWidth()) /
                static_cast<Ogre::Real>(mViewport->getActualHeight());
        }

        const Ogre::Real distance = viewingDistance(
            mExtents,
            mBounds,
            aspect,
            mCamera->getFOVy());

        mCameraNode->setPosition(0.0f, 0.0f, distance);
        mCamera->setNearClipDistance(
            std::max<Ogre::Real>(
                static_cast<Ogre::Real>(0.01),
                distance * static_cast<Ogre::Real>(0.01)));
        mCamera->setFarClipDistance(distance * static_cast<Ogre::Real>(4));
    }

    bool frameStarted(const Ogre::FrameEvent& evt) override
    {
        mMeshNode->yaw(
            Ogre::Degree(20.0f * evt.timeSinceLastFrame),
            Ogre::Node::TS_PARENT);

        updateCamera();
        return true;
    }

private:
    Ogre::SceneNode* mMeshNode;
    Ogre::SceneNode* mCameraNode;
    Ogre::Camera* mCamera;
    Ogre::Viewport* mViewport;
    Ogre::AxisAlignedBox mBounds;
    std::vector<OrbitExtent> mExtents;
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

    lightNode->attachObject(light);

    Ogre::Entity* monster =
        sceneManager->createEntity("ogrehead.mesh");

    const Ogre::AxisAlignedBox bounds = monster->getMesh()->getBounds();
    const Ogre::Vector3 center = bounds.getCenter();
    const Ogre::Real radius = bounds.getHalfSize().length();

    Ogre::SceneNode* monsterNode =
        sceneManager->getRootSceneNode()
                    ->createChildSceneNode();

    monsterNode->attachObject(monster);
    monsterNode->setPosition(-center);

    lightNode->setPosition(
        radius * 2.0f,
        radius * 4.0f,
        radius * 5.0f);

    Ogre::Camera* camera =
        sceneManager->createCamera("MainCamera");

    camera->setAutoAspectRatio(true);

    Ogre::SceneNode* cameraNode =
        sceneManager->getRootSceneNode()
                    ->createChildSceneNode();

    cameraNode->lookAt(
        Ogre::Vector3::ZERO,
        Ogre::Node::TS_PARENT);

    cameraNode->attachObject(camera);

    Ogre::Viewport* viewport =
        app.getRenderWindow()->addViewport(camera);

    viewport->setBackgroundColour(
        Ogre::ColourValue(0.08f, 0.12f, 0.20f));

    SpinningMeshView view(
        monsterNode,
        cameraNode,
        camera,
        viewport,
        bounds,
        meshExtents(monster, center));

    view.updateCamera();
    root->addFrameListener(&view);

    root->startRendering();

    app.closeApp();
    return 0;
}

