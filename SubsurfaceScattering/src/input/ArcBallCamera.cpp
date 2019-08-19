#include "ArcBallCamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>

sss::ArcBallCamera::ArcBallCamera(const glm::vec3 &center, float distance)
	:m_center(center),
	m_distance(distance),
	m_theta(glm::half_pi<float>()),
	m_phi()
{
}

void sss::ArcBallCamera::update(const glm::vec2 &mouseDelta, float scrollDelta)
{
	const float SCROLL_DELTA_MULT = -0.1f;
	const float MOUSE_DELTA_MULT = -0.01f;
	m_distance += scrollDelta * SCROLL_DELTA_MULT * m_distance;
	m_distance = glm::max(0.0f, m_distance);

	m_theta = glm::clamp(m_theta + mouseDelta.y * MOUSE_DELTA_MULT, 0.0001f, glm::pi<float>() - 0.0001f);
	m_phi += mouseDelta.x * MOUSE_DELTA_MULT;
}

glm::mat4 sss::ArcBallCamera::getViewMatrix() const
{
	return glm::lookAt(getPosition(), m_center, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 sss::ArcBallCamera::getPosition() const
{
	return  glm::vec3(m_distance * glm::sin(m_theta) * glm::sin(m_phi),
		m_distance * glm::cos(m_theta),
		m_distance * glm::sin(m_theta) * glm::cos(m_phi)) + m_center;
}
