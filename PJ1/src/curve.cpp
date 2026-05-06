#include "curve.h"
#include "vertexrecorder.h"
#include <cmath>
using namespace std;

const float c_pi = 3.14159265358979323846f;

// Runtime flag: when true, swap the colors used for B and T axes in
// recordCurveFrames. Set via setSwapCurveBG() (command-line flag in main.cpp).
static bool g_swapCurveBG = false;

void setSwapCurveBG(bool swap) { g_swapCurveBG = swap; }

namespace
{
// Approximately equal to.  We don't want to use == because of
// precision issues with floating point.
inline bool approx(const Vector3f& lhs, const Vector3f& rhs)
{
	const float eps = 1e-8f;
	return (lhs - rhs).absSquared() < eps;
}

bool isCurve2D(const vector<Vector3f>& P)
{
	for (size_t i = 0; i < P.size(); i++)
		if (P[i].z() != 0.0f) return false;
	return true;
}

void computeFrames(Curve& curve, bool flat)
{
	if (curve.empty()) return;

	if (flat) {
		Vector3f B(0, 0, 1);
		for (size_t i = 0; i < curve.size(); i++) {
			curve[i].B = B;
			curve[i].N = Vector3f::cross(B, curve[i].T).normalized();
		}
		return;
	}

	// 3D: initialize first frame.
	// Use the projection of world +Z onto the plane perpendicular to T0 as B0.
	// This matches sample_solution/athena/a1. (pj1.md slide 12 allows an arbitrary
	// B0 as long as it is not parallel to T1; the (0,0,1) x T1 form there is only
	// an example.) With the projection form, the distribution of the closure-
	// correction rotation better preserves features such as concavities (e.g. the
	// middle dip of weirder.swp).
	Vector3f T0 = curve[0].T;
	const Vector3f kz(0, 0, 1);
	Vector3f B0 = kz - Vector3f::dot(kz, T0) * T0;
	if (B0.absSquared() < 1e-8f) {
		const Vector3f kx(1, 0, 0);
		B0 = kx - Vector3f::dot(kx, T0) * T0;
	}
	B0.normalize();
	curve[0].N = Vector3f::cross(B0, T0).normalized();
	curve[0].B = B0;

	// Recursive frame propagation
	for (size_t i = 1; i < curve.size(); i++) {
		Vector3f Ti = curve[i].T;
		Vector3f Nprev = curve[i - 1].N;
		Vector3f Ntmp = Nprev - Vector3f::dot(Nprev, Ti) * Ti;
		curve[i].N = Ntmp.normalized();
		curve[i].B = Vector3f::cross(Ti, curve[i].N).normalized();
	}

	// Closure correction for closed curves
	if (curve.size() > 1 && approx(curve.front().V, curve.back().V)) {
		Vector3f Nfirst = curve[0].N;
		Vector3f Nlast = curve.back().N;
		Vector3f Blast = curve.back().B;

		float cosA = Vector3f::dot(Nfirst, Nlast);
		cosA = max(-1.0f, min(1.0f, cosA));
		float sinA = Vector3f::dot(Nfirst, Blast);
		float alpha = atan2(sinA, cosA);

		int n = (int)curve.size();
		for (int i = 0; i < n; i++) {
			float theta = alpha * (float)i / (float)(n - 1);
			float ct = cos(theta);
			float st = sin(theta);
			Vector3f Nold = curve[i].N;
			Vector3f Bold = curve[i].B;
			curve[i].N = ct * Nold + st * Bold;
			curve[i].B = -st * Nold + ct * Bold;
		}
	}
}

}


Curve evalBezier(const vector< Vector3f >& P, unsigned steps)
{
	if (P.size() < 4 || P.size() % 3 != 1)
	{
		cerr << "evalBezier must be called with 3n+1 control points." << endl;
		return Curve();
	}

	bool flat = isCurve2D(P);
	int numSegments = ((int)P.size() - 1) / 3;

	Curve curve;
	for (int seg = 0; seg < numSegments; seg++) {
		const Vector3f& P0 = P[3 * seg];
		const Vector3f& P1 = P[3 * seg + 1];
		const Vector3f& P2 = P[3 * seg + 2];
		const Vector3f& P3 = P[3 * seg + 3];

		unsigned startI = (seg == 0) ? 0 : 1;
		for (unsigned i = startI; i <= steps; i++) {
			float t = (float)i / (float)steps;
			float mt = 1.0f - t;

			CurvePoint cp;
			cp.V = mt * mt * mt * P0
				+ 3.0f * mt * mt * t * P1
				+ 3.0f * mt * t * t * P2
				+ t * t * t * P3;

			cp.T = (3.0f * mt * mt * (P1 - P0)
				+ 6.0f * mt * t * (P2 - P1)
				+ 3.0f * t * t * (P3 - P2)).normalized();

			curve.push_back(cp);
		}
	}

	computeFrames(curve, flat);
	return curve;
}

Curve evalBspline(const vector< Vector3f >& P, unsigned steps)
{
	if (P.size() < 4)
	{
		cerr << "evalBspline must be called with 4 or more control points." << endl;
		return Curve();
	}

	// Convert B-spline to Bezier using basis change: G_bez = M_bez^{-1} * M_bsp * G_bsp
	// The conversion matrix for each segment's 4 control points is:
	// Q0 = (1*Pj + 4*Pj1 + 1*Pj2) / 6
	// Q1 = (0*Pj + 4*Pj1 + 2*Pj2) / 6
	// Q2 = (0*Pj + 2*Pj1 + 4*Pj2) / 6
	// Q3 = (0*Pj + 1*Pj1 + 4*Pj2 + 1*Pj3) / 6
	int numSegments = (int)P.size() - 3;
	vector<Vector3f> bezP;

	for (int j = 0; j < numSegments; j++) {
		Vector3f Q0 = (P[j] + 4.0f * P[j + 1] + P[j + 2]) / 6.0f;
		Vector3f Q1 = (4.0f * P[j + 1] + 2.0f * P[j + 2]) / 6.0f;
		Vector3f Q2 = (2.0f * P[j + 1] + 4.0f * P[j + 2]) / 6.0f;
		Vector3f Q3 = (P[j + 1] + 4.0f * P[j + 2] + P[j + 3]) / 6.0f;

		if (j == 0)
			bezP.push_back(Q0);
		bezP.push_back(Q1);
		bezP.push_back(Q2);
		bezP.push_back(Q3);
	}

	return evalBezier(bezP, steps);
}

Curve evalCircle(float radius, unsigned steps)
{
	// This is a sample function on how to properly initialize a Curve
	// (which is a vector< CurvePoint >).

	// Preallocate a curve with steps+1 CurvePoints
	Curve R(steps + 1);

	// Fill it in counterclockwise
	for (unsigned i = 0; i <= steps; ++i)
	{
		// step from 0 to 2pi
		float t = 2.0f * c_pi * float(i) / steps;

		// Initialize position
		// We're pivoting counterclockwise around the y-axis
		R[i].V = radius * Vector3f(cos(t), sin(t), 0);

		// Tangent vector is first derivative
		R[i].T = Vector3f(-sin(t), cos(t), 0);

		// Normal vector is second derivative
		R[i].N = Vector3f(-cos(t), -sin(t), 0);

		// Finally, binormal is facing up.
		R[i].B = Vector3f(0, 0, 1);
	}

	return R;
}

void recordCurve(const Curve& curve, VertexRecorder* recorder)
{
	const Vector3f WHITE(1, 1, 1);
	for (int i = 0; i < (int)curve.size() - 1; ++i)
	{
		recorder->record_poscolor(curve[i].V, WHITE);
		recorder->record_poscolor(curve[i + 1].V, WHITE);
	}
}
void recordCurveFrames(const Curve& curve, VertexRecorder* recorder, float framesize)
{
	Matrix4f T;
	const Vector3f RED(1, 0, 0);
	const Vector3f GREEN(0, 1, 0);
	const Vector3f BLUE(0, 0, 1);

	// Per pj1.md slide 8: N=red, T=green, B=blue.
	// With --swap-bg, swap B/T colors: N=red, B=green, T=blue (matches sample).
	const Vector3f colorB = g_swapCurveBG ? GREEN : BLUE;
	const Vector3f colorT = g_swapCurveBG ? BLUE  : GREEN;

	const Vector4f ORGN(0, 0, 0, 1);
	const Vector4f AXISX(framesize, 0, 0, 1);
	const Vector4f AXISY(0, framesize, 0, 1);
	const Vector4f AXISZ(0, 0, framesize, 1);

	for (int i = 0; i < (int)curve.size(); ++i)
	{
		T.setCol(0, Vector4f(curve[i].N, 0));
		T.setCol(1, Vector4f(curve[i].B, 0));
		T.setCol(2, Vector4f(curve[i].T, 0));
		T.setCol(3, Vector4f(curve[i].V, 1));
 
		// Transform orthogonal frames into model space
		Vector4f MORGN  = T * ORGN;
		Vector4f MAXISX = T * AXISX;
		Vector4f MAXISY = T * AXISY;
		Vector4f MAXISZ = T * AXISZ;

		// Record in model space
		// N (column 0, AXISX direction) -> RED
		recorder->record_poscolor(MORGN.xyz(), RED);
		recorder->record_poscolor(MAXISX.xyz(), RED);

		// B (column 1, AXISY direction) -> BLUE  (or GREEN if --swap-bg)
		recorder->record_poscolor(MORGN.xyz(), colorB);
		recorder->record_poscolor(MAXISY.xyz(), colorB);

		// T (column 2, AXISZ direction) -> GREEN (or BLUE if --swap-bg)
		recorder->record_poscolor(MORGN.xyz(), colorT);
		recorder->record_poscolor(MAXISZ.xyz(), colorT);
	}
}

