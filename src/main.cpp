#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "bevgrafmath2017.h"


struct color_t {
	uint8_t r, g, b, a = 255;
};

void glColorCl(const color_t &cl) {
	glColor4ub(cl.r, cl.g, cl.b, cl.a);
}

void glClearColorCl(const color_t &cl) {
	glClearColor((float)cl.r / 255.f, (float)cl.g / 255.f, (float)cl.b / 255.f, (float)cl.a / 255.f);
}

const color_t BACKGROUND_COLOR = { 40, 40, 40 };
const color_t CONTROL_POINT_COLOR = { 255, 255, 255 };
const color_t CONTROL_POLYGON_COLOR = { 255, 255, 255 };
const color_t CURVE_COLOR = { 255, 171, 64 };

const float CURVE_WIDTH = 2.5f;
const float CONTROL_POLYGON_WIDTH = 1.5f;
const float CONTROL_POINT_SIZE = 4.0f;

const float CLICK_THRESHOLD = 100.0f;

const float EVALUATION_PARAMETER_DELTA = 0.05f;

const size_t MINIMUM_NUMBER_OF_CURVE_CONTROL_POINTS = 4;

std::vector<vec2> controlPoints;
vec2 *draggedControlPoint = nullptr;

float tension = 0.0f;
float bias = 0.0f;
float continuity = 0.0f;



mat4 calculateCoefficientMatrix(const float tension, const float bias, const float continuity);
void drawCurve(const mat4 &coefficientMatrix, const std::vector<vec2> &controlPoints);
void drawSegment(const size_t segmentIndex, const mat4 &coefficientMatrix, const std::vector<vec2> &controlPoints);
void drawControlPolygon(const std::vector<vec2> &controlPoints);
void drawControlPoints(const std::vector<vec2> &controlPoints);

vec2 *getClickedPoint(const vec2 &cursorPosition, std::vector<vec2> &controlPoints);

GLFWwindow *createWindow();
void setupFrameBuffer(GLFWwindow *window);

void setupInputCallbacks(GLFWwindow *window);
void onMouseMove(GLFWwindow *window, double x, double y);
void onMouseButton(GLFWwindow *window, int button, int action, int modifies);

int main(int argc, char **argv) {
	if (glfwInit() == GLFW_FALSE) {
		return 1;
	}

	glfwSetTime(0);

	GLFWwindow *window = createWindow();
	if (window == nullptr) {
		glfwTerminate();
		return 2;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(0);
	
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		glfwTerminate();
		return 3;
	}

	setupFrameBuffer(window);
	setupInputCallbacks(window);
	
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		glClearColorCl(BACKGROUND_COLOR);
		glClear(GL_COLOR_BUFFER_BIT);

		if (controlPoints.size() >= MINIMUM_NUMBER_OF_CURVE_CONTROL_POINTS) {
			const mat4 coefficientMatrix = calculateCoefficientMatrix(tension, bias, continuity);

			drawCurve(coefficientMatrix, controlPoints);
		}

		drawControlPolygon(controlPoints);

		drawControlPoints(controlPoints);

		glfwSwapBuffers(window);
	}

	glfwTerminate();

    return 0;
}

void setupInputCallbacks(GLFWwindow *window) {
	glfwSetCursorPosCallback(window, onMouseMove);

	glfwSetMouseButtonCallback(window, onMouseButton);
}

void onMouseMove(GLFWwindow *window, double x, double y) {
	if (draggedControlPoint) {
		draggedControlPoint->x = (float)x;
		draggedControlPoint->y = (float)y;
	}
}

void onMouseButton(GLFWwindow *window, int button, int action, int modifies) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_RELEASE) {
			draggedControlPoint = nullptr;
		}
		else if (action == GLFW_PRESS) {
			double x, y;
			glfwGetCursorPos(window, &x, &y);

			const vec2 cursorPosition = { (float)x, (float)y };

			vec2 *pointUnderCursor = getClickedPoint(cursorPosition, controlPoints);

			if (pointUnderCursor == nullptr) {
				controlPoints.push_back(cursorPosition);
			}
			else {
				draggedControlPoint = pointUnderCursor;
			}
		}
	}
}

GLFWwindow *createWindow() {
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	return glfwCreateWindow(1024, 768, "Kochanek-Bartels Spline", nullptr, nullptr);
}

void setupFrameBuffer(GLFWwindow *window) {
	int frameBufferWidth, frameBufferHeight;
	glfwGetFramebufferSize(window, &frameBufferWidth, &frameBufferHeight);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.f, frameBufferWidth, frameBufferHeight, 0.f, 0.f, 1.f);
}

mat4 calculateCoefficientMatrix(const float tension, const float bias, const float continuity) {
	const float s = 0.5f * (1.0f - tension);
	const float q1 = s * (1.0f + bias) * (1.0f - continuity);
	const float q2 = s * (1.0f - bias) * (1.0f + continuity);
	const float q3 = s * (1.0f + bias) * (1.0f + continuity);
	const float q4 = s * (1.0f - bias) * (1.0f - continuity);

	return {
		{ -q1, 2.0f * q1, -q1, 0 },
		{ q1 - q2 - q3 + 2.0f, q3 - (2.0f * q1) + (2.0f * q2) - 3.0f, q1 - q2, 1.0f },
		{ q2 + q3 - q4 - 2.0f, q4 - q3 - (2.0f * q2) + 3.0f, q2, 0 },
		{ q4, -q4, 0, 0}
	};
}

void drawCurve(const mat4 &coefficientMatrix, const std::vector<vec2> &controlPoints) {
	const size_t segmentCount = controlPoints.size() - 3;

	glLineWidth(CURVE_WIDTH);
	glColorCl(CURVE_COLOR);
	glBegin(GL_LINE_STRIP);
	for (size_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
		drawSegment(segmentIndex, coefficientMatrix, controlPoints);
	}
	glEnd();
}

void drawSegment(const size_t segmentIndex, const mat4 &coefficientMatrix, const std::vector<vec2> &controlPoints) {
	const mat24 geometry = {
		controlPoints[segmentIndex + 0],
		controlPoints[segmentIndex + 1],
		controlPoints[segmentIndex + 2],
		controlPoints[segmentIndex + 3]
	};

	const mat24 gm = geometry * coefficientMatrix;

	for (float t = 0.0f; t <= 1.0f + EVALUATION_PARAMETER_DELTA; t += EVALUATION_PARAMETER_DELTA) {
		const vec4 parameterVector = { t * t * t, t * t, t, 1.0f };

		const vec2 curvePoint = gm * parameterVector;

		glVertex2f(curvePoint.x, curvePoint.y);
	}
}

void drawControlPolygon(const std::vector<vec2> &controlPoints) {
	glLineWidth(CONTROL_POLYGON_WIDTH);
	glColorCl(CONTROL_POLYGON_COLOR);
	glColor3ub(255, 255, 255);
	glBegin(GL_LINE_STRIP);
	for (const auto& point : controlPoints) {
		glVertex2f(point.x, point.y);
	}
	glEnd();
}

void drawControlPoints(const std::vector<vec2> &controlPoints) {
	glPointSize(CONTROL_POINT_SIZE);
	glColorCl(CONTROL_POINT_COLOR);
	glBegin(GL_POINTS);
	for (const auto &point : controlPoints) {
		glVertex2f(point.x, point.y);
	}
	glEnd();
}

vec2 *getClickedPoint(const vec2 &cursorPosition, std::vector<vec2> &controlPoints) {
	const auto clickedIterator = std::find_if(controlPoints.begin(), controlPoints.end(), [&cursorPosition](const vec2 &point) {
		return dist2(cursorPosition, point) <= CLICK_THRESHOLD;
	});

	return clickedIterator == controlPoints.end()
		? nullptr
		: &(*clickedIterator);
}
