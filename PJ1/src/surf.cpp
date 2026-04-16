#include "surf.h"
#include "vertexrecorder.h"
#include <cmath>
using namespace std;

const float s_pi = 3.14159265358979323846f;

namespace
{
    
    // We're only implenting swept surfaces where the profile curve is
    // flat on the xy-plane.  This is a check function.
    static bool checkFlat(const Curve &profile)
    {
        for (unsigned i=0; i<profile.size(); i++)
            if (profile[i].V[2] != 0.0 ||
                profile[i].T[2] != 0.0 ||
                profile[i].N[2] != 0.0)
                return false;
    
        return true;
    }

    void buildTriangles(Surface &surface, unsigned numSlices, unsigned numProfile)
    {
        for (unsigned i = 0; i < numSlices - 1; i++) {
            for (unsigned j = 0; j < numProfile - 1; j++) {
                unsigned A = i * numProfile + j;
                unsigned B = i * numProfile + j + 1;
                unsigned C = (i + 1) * numProfile + j;
                unsigned D = (i + 1) * numProfile + j + 1;
                surface.VF.push_back(Tup3u(A, B, C));
                surface.VF.push_back(Tup3u(C, B, D));
            }
        }
    }
}

Surface makeSurfRev(const Curve &profile, unsigned steps)
{
    Surface surface;
    
    if (!checkFlat(profile))
    {
        cerr << "surfRev profile curve must be flat on xy plane." << endl;
        exit(0);
    }

    unsigned numProfile = profile.size();

    for (unsigned i = 0; i <= steps; i++) {
        float theta = 2.0f * s_pi * (float)i / (float)steps;
        float ct = cos(theta);
        float st = sin(theta);

        for (unsigned j = 0; j < numProfile; j++) {
            Vector3f V = profile[j].V;
            surface.VV.push_back(Vector3f(V.x() * ct, V.y(), -V.x() * st));

            Vector3f N = profile[j].N;
            surface.VN.push_back(Vector3f(-N.x() * ct, -N.y(), N.x() * st));
        }
    }

    buildTriangles(surface, steps + 1, numProfile);
    return surface;
}

Surface makeGenCyl(const Curve &profile, const Curve &sweep )
{
    Surface surface;

    if (!checkFlat(profile))
    {
        cerr << "genCyl profile curve must be flat on xy plane." << endl;
        exit(0);
    }

    unsigned numProfile = profile.size();
    unsigned numSweep = sweep.size();

    for (unsigned i = 0; i < numSweep; i++) {
        Vector3f Ns = sweep[i].N;
        Vector3f Bs = sweep[i].B;
        Vector3f Vs = sweep[i].V;

        for (unsigned j = 0; j < numProfile; j++) {
            float px = profile[j].V.x();
            float py = profile[j].V.y();
            surface.VV.push_back(Vs + px * Ns + py * Bs);

            float nx = profile[j].N.x();
            float ny = profile[j].N.y();
            Vector3f sn = -(nx * Ns + ny * Bs);
            surface.VN.push_back(sn.normalized());
        }
    }

    buildTriangles(surface, numSweep, numProfile);
    return surface;
}

void recordSurface(const Surface &surface, VertexRecorder* recorder) {
	const Vector3f WIRECOLOR(0.4f, 0.4f, 0.4f);
    for (int i=0; i<(int)surface.VF.size(); i++)
    {
		recorder->record(surface.VV[surface.VF[i][0]], surface.VN[surface.VF[i][0]], WIRECOLOR);
		recorder->record(surface.VV[surface.VF[i][1]], surface.VN[surface.VF[i][1]], WIRECOLOR);
		recorder->record(surface.VV[surface.VF[i][2]], surface.VN[surface.VF[i][2]], WIRECOLOR);
    }
}

void recordNormals(const Surface &surface, VertexRecorder* recorder, float len)
{
	const Vector3f NORMALCOLOR(0, 1, 1);
    for (int i=0; i<(int)surface.VV.size(); i++)
    {
		recorder->record_poscolor(surface.VV[i], NORMALCOLOR);
		recorder->record_poscolor(surface.VV[i] + surface.VN[i] * len, NORMALCOLOR);
    }
}

void outputObjFile(ostream &out, const Surface &surface)
{
    
    for (int i=0; i<(int)surface.VV.size(); i++)
        out << "v  "
            << surface.VV[i][0] << " "
            << surface.VV[i][1] << " "
            << surface.VV[i][2] << endl;

    for (int i=0; i<(int)surface.VN.size(); i++)
        out << "vn "
            << surface.VN[i][0] << " "
            << surface.VN[i][1] << " "
            << surface.VN[i][2] << endl;

    out << "vt  0 0 0" << endl;
    
    for (int i=0; i<(int)surface.VF.size(); i++)
    {
        out << "f  ";
        for (unsigned j=0; j<3; j++)
        {
            unsigned a = surface.VF[i][j]+1;
            out << a << "/" << "1" << "/" << a << " ";
        }
        out << endl;
    }
}
