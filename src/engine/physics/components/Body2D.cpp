#include "components/Body2D.h"
#include "components/Transforms.h"
#include "PhysicsWorld.h"
#include "Constants.h"
#include "Body2DUserData.h"

#include <box2d/box2d.h>

void Body2D::addTopViewFriction(float normalForce) {
    b2BodyDef frictionBodyDef;
    b2FixtureDef fixtureDef;
    fixtureDef.isSensor = true;
    m_frictionBody = m_world->CreateBody(&frictionBodyDef);
    b2FrictionJointDef jointDef;
    jointDef.bodyA = m_frictionBody;
    jointDef.bodyB = m_body;
    jointDef.maxForce = normalForce * frictionCoefficient;
    /* Setting maxTorque to same as maxForce might not be realistic... (but works for now) */
    jointDef.maxTorque = normalForce * frictionCoefficient;
    /* No need to explicitly delete joint, it's deleted when the attached body is deleted */
    m_world->CreateJoint(&jointDef);
}

Body2D::Body2D(const PhysicsWorld &world, QuadTransform *transform, const Body2D::BodySpec &bodySpec) :
    PhysicsComponent(world)
{
    assert(transform != nullptr);
    transform->size.x = PhysicsWorld::scaleLength(transform->size.x);
    transform->size.y = PhysicsWorld::scaleLength(transform->size.y);
    transform->position.x = PhysicsWorld::scalePosition(transform->position.x);
    transform->position.y = PhysicsWorld::scalePosition(transform->position.y);
    const float scaledMass = PhysicsWorld::scaleMass(bodySpec.massUnscaled);
    const float normalForce = PhysicsWorld::normalForce(bodySpec.massUnscaled);
    const float area = transform->size.x * transform->size.y;
    const float density = scaledMass / area;

    b2BodyDef bodyDef;
    bodyDef.type = bodySpec.dynamic ? b2_dynamicBody : b2_staticBody;
    bodyDef.position = b2Vec2(transform->position.x, transform->position.y);
    bodyDef.angle = transform->rotation;
    m_body = m_world->CreateBody(&bodyDef);

    b2FixtureDef fixtureDef;
    b2PolygonShape polygonShape;
    polygonShape.SetAsBox(transform->size.x / 2, transform->size.y / 2);
    fixtureDef.shape = &polygonShape;
    fixtureDef.isSensor = !bodySpec.collision;
    fixtureDef.density = density;
    m_body->CreateFixture(&fixtureDef);

    m_translator = std::make_unique<QuadTransformTranslator>(transform, *m_body);

    if (world.getGravityType() == PhysicsWorld::Gravity::TopView) {
        addTopViewFriction(normalForce);
    }
}

void Body2D::QuadTransformTranslator::translate()
{
    const b2Vec2 position = m_body->GetPosition();
    m_transform->position.x = position.x;
    m_transform->position.y = position.y;
    m_transform->rotation = m_body->GetAngle();
}

constexpr static int totalVertexCount = 180;
constexpr static float anglePerVertex = 2 * constants::pi / static_cast<float>(totalVertexCount);
constexpr static int trapezoidVertexCount = 4;
Body2D::Body2D(const PhysicsWorld &world, HollowCircleTransform *transform, const Body2D::BodySpec &bodySpec) :
    PhysicsComponent(world)
{
    assert(transform != nullptr);
    /* Only support static for now */
    assert(!bodySpec.dynamic);
    assert(world.getGravityType() == PhysicsWorld::Gravity::TopView);
    assert(transform->outerRadius > transform->innerRadius);
    transform->innerRadius = PhysicsWorld::scaleLength(transform->innerRadius);
    transform->outerRadius = PhysicsWorld::scaleLength(transform->outerRadius);
    transform->position.x = PhysicsWorld::scalePosition(transform->position.x);
    transform->position.y = PhysicsWorld::scalePosition(transform->position.y);

    b2BodyDef bodyDef;
    bodyDef.type = b2_staticBody;
    bodyDef.position = b2Vec2(transform->position.x, transform->position.y);

    m_body = m_world->CreateBody(&bodyDef);
    b2PolygonShape shape;
    b2Vec2 trapezoidVertices[trapezoidVertexCount];
    float angle=0;
    for (int cornerIdx=0; cornerIdx < totalVertexCount; cornerIdx++) {
        angle = cornerIdx * anglePerVertex;
        trapezoidVertices[0].Set(transform->innerRadius * cosf(angle), transform->innerRadius * sinf(angle));
        trapezoidVertices[1].Set(transform->outerRadius * cosf(angle), transform->outerRadius * sinf(angle));
        angle += anglePerVertex;
        trapezoidVertices[2].Set(transform->outerRadius * cosf(angle), transform->outerRadius * sinf(angle));
        trapezoidVertices[3].Set(transform->innerRadius * cosf(angle), transform->innerRadius * sinf(angle));
        shape.Set(trapezoidVertices, trapezoidVertexCount);
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &shape;
        fixtureDef.isSensor = !bodySpec.collision;
        m_body->CreateFixture(&fixtureDef);
    }

    /* Only static so no translation needed for now */
    m_translator = nullptr;
}

void Body2D::CircleTransformTranslator::translate()
{
}

void Body2D::setUserData(Body2DUserData *userData) {
    m_body->GetUserData().pointer = reinterpret_cast<uintptr_t>(userData);
}

glm::vec2 Body2D::getPosition() const
{
    auto position = m_body->GetPosition();
    return { position.x, position.y };
}

float Body2D::getAngle() const
{
    return m_body->GetAngle();
}

float Body2D::getForwardSpeed() const
{
    b2Vec2 currentForwardNormal = m_body->GetWorldVector(b2Vec2(0,1));
    return b2Dot(currentForwardNormal, m_body->GetLinearVelocity());
}

glm::vec2 Body2D::getLateralVelocity() const
{
    b2Vec2 currentRightNormal = m_body->GetWorldVector( b2Vec2(1,0) );
    b2Vec2 lateralVelocity = b2Dot(currentRightNormal, m_body->GetLinearVelocity()) * currentRightNormal;
    return { lateralVelocity.x, lateralVelocity.y };
}

glm::vec2 Body2D::getForwardNormal() const
{
    const b2Vec2 forwardNormal = m_body->GetWorldVector(b2Vec2(0,1));
    return glm::vec2{ forwardNormal.x, forwardNormal.y };
}

void Body2D::setForce(const glm::vec2 &vec, float magnitude)
{
    m_body->ApplyForce(magnitude * b2Vec2(vec.x, vec.y), m_body->GetWorldCenter(), true);
}

void Body2D::setLinearImpulse(const glm::vec2 &vec)
{
    b2Vec2 box2DVec(vec.x, vec.y);
    m_body->ApplyLinearImpulse(b2Vec2(vec.x, vec.y), m_body->GetWorldCenter(), true);
}

float Body2D::getMass() const
{
    assert(m_body);
    return m_body->GetMass();
}

void Body2D::onFixedUpdate(double stepTime)
{
    if (nullptr == m_translator) {
        return;
    }
    m_translator->translate();
}

void Body2D::attachBodyWithRevoluteJoint(const glm::vec2 &unscaledAttachPos, const Body2D *body)
{
    b2RevoluteJointDef jointDef;
    jointDef.bodyA = m_body;
    jointDef.enableLimit = true;
    jointDef.lowerAngle = 0;
    jointDef.upperAngle = 0;
    /* Center of the body to be attached */
    jointDef.localAnchorB.SetZero();
    jointDef.bodyB = body->m_body;
    jointDef.localAnchorA.Set(PhysicsWorld::scalePosition(unscaledAttachPos.x),
                              PhysicsWorld::scalePosition(unscaledAttachPos.y));
    /* No need to explicitly delete joint, it's deleted when the attached body is deleted */
    m_world->CreateJoint(&jointDef);
}

void Body2D::attachBodyWithWeldJoint(const glm::vec2 &unscaledAttachPos, const Body2D *body)
{
    b2WeldJointDef jointDef;
    jointDef.bodyA = m_body;
    jointDef.localAnchorA.Set(PhysicsWorld::scalePosition(unscaledAttachPos.x),
                              PhysicsWorld::scalePosition(unscaledAttachPos.y));
    jointDef.localAnchorB.SetZero();
    jointDef.bodyB = body->m_body;
    /* No need to explicitly delete joint, it's deleted when the attached body is deleted */
    m_world->CreateJoint(&jointDef);
}

Body2D::~Body2D()
{
    m_world->DestroyBody(m_body);
    if (m_frictionBody != nullptr) {
        m_world->DestroyBody(m_frictionBody);
    }
}