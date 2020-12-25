#include "SpriteAnimationTestScene.h"
#include "SpriteAnimation.h"
#include "shapes/QuadObject.h"

#include <glm/glm.hpp>

SpriteAnimationTestScene::SpriteAnimationTestScene() :
    Scene("Simple animation sprite sheet test")
{
    const SpriteAnimation::Params animationParams = { 1, 2, 2, 30 };
    m_quadObject = std::make_unique<QuadObject>(this, "AnimationTestSpriteSheet.png", &animationParams,
                                                nullptr, glm::vec2{ 0.0f, 0.0f }, glm::vec2{ 0.25f, 0.25f }, 0.0f);
}
