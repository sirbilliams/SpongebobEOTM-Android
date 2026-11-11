#include "WalkRoute.h"
#include <cmath>
#include <cstring>

namespace sbwalk {
static const double pi = 3.14159265358979323846;
static double norm(double a)
{
    while (a >= 2*pi) a -= 2*pi;
    while (a < 0) a += 2*pi;
    return a;
}

Piece::Piece() { std::memset(this, 0, sizeof *this); }
Grid::Grid() : allowOutside(true), width(0.1), step(0.2)
{ std::memset(blocked, 0, sizeof blocked); }
double Path::length() const
{
    return pieces[0].length + pieces[1].length + pieces[2].length + pieces[3].length;
}

// 0x42dc90: strict '>' at piece boundaries, clockwise subtracts distance/radius.
Node Path::at(double distance) const
{
    if (distance >= length()) return end;
    int i = 0;
    while (i < 2 && distance > pieces[i].length) distance -= pieces[i++].length;
    const Piece& p = pieces[i];
    if (!p.arc) return Node(p.x + std::cos(p.angle)*distance,
                            p.z + std::sin(p.angle)*distance, p.angle);
    double a = p.angle + (p.clockwise ? -distance : distance)/radius;
    return Node(p.cx + std::cos(a)*radius, p.cz + std::sin(a)*radius,
                norm(a + (p.clockwise ? -pi/2 : pi/2)));
}

// 0x42cbb0: x87 after subtractions [dz,dx], square/add/sqrt then fxch/fpatan.
Path straight(const Node& start, const Node& end)
{
    Path p;
    double dx = end.x-start.x, dz = end.z-start.z;
    p.pieces[0].x = start.x; p.pieces[0].z = start.z;
    p.pieces[0].length = std::sqrt(dx*dx + dz*dz);
    p.pieces[0].angle = std::atan2(dz,dx);
    p.end = Node(end.x,end.z,p.pieces[0].angle);
    return p;
}

static void circle(Piece& p, const Node& n, bool cw, double r)
{
    p.arc = 1; p.clockwise = cw;
    p.angle = norm(n.heading + (cw ? pi/2 : -pi/2));
    p.cx = n.x-std::cos(p.angle)*r; p.cz = n.z-std::sin(p.angle)*r;
}

// 0x42d5f0, called twice by 0x42cc50. At d6e5 the stack is [r,d];
// fld st1/fmul st2 -> [d*d,r,d]; fld st1/fmul st2 -> [r*r,d*d,r,d];
// fsubp/fsqrt -> sqrt(d*d-r*r). 0x443a80 is the CRT acos wrapper.
static bool arcLine(const Node& start, const Node& end, bool cw, double r, Path& p)
{
    p = Path(); p.radius = r;
    Piece& a = p.pieces[0]; Piece& line = p.pieces[1];
    circle(a,start,cw,r);
    double dx = end.x-a.cx, dz = end.z-a.cz;
    double d = std::sqrt(dx*dx+dz*dz);
    if (d < r) return false;
    line.length = std::sqrt(d*d-r*r);
    // d710..d724: atan2(dz,dx) +/- acos(r/d), NOT atan2(dx,dz).
    double tangent = std::atan2(dz,dx) + (cw ? 1 : -1)*std::acos(r/d);
    a.sweep = norm(cw ? a.angle-tangent : tangent-a.angle);
    a.length = a.sweep*r;
    line.x = a.cx+std::cos(tangent)*r; line.z = a.cz+std::sin(tangent)*r;
    line.angle = norm(tangent+(cw ? -pi/2 : pi/2));
    p.end = Node(end.x,end.z,line.angle);
    return true;
}

// 0x42cc50: clockwise first, strict smaller length wins (ties retain first).
bool turnToPoint(const Node& start, const Node& end, double radius, Path& out)
{
    if (radius <= 0) return false; // port: reject invalid caller input.
    double best = 9999999; bool found = false;
    for (int i = 0; i < 2; ++i) {
        Path p;
        if (arcLine(start,end,i == 0,radius,p) && p.length() < best) {
            best = p.length(); out = p; found = true;
        }
    }
    // TODO(0x42d090): optional eight-direction straight-piece splitting (+39a,
    // default false at 0x42a6c0) is not enabled by this port.
    return found;
}

// 0x42cec0 + 0x42d870: common tangent of two equal-radius turning circles.
static bool arcLineArc(const Node& start, const Node& end, bool cw1, bool cw2,
                       double r, Path& p)
{
    p = Path(); p.radius = r;
    Piece& a = p.pieces[0]; Piece& line = p.pieces[1]; Piece& b = p.pieces[2];
    circle(a,start,cw1,r); circle(b,end,cw2,r);
    double endRadial = b.angle;
    double dx = b.cx-a.cx, dz = b.cz-a.cz;
    double d = std::sqrt(dx*dx+dz*dz), theta = std::atan2(dz,dx), tangent;
    if (cw1 == cw2) {
        tangent = norm(theta+(cw1 ? pi/2 : -pi/2)); line.length = d;
    } else {
        if (d == 0 || std::fabs(2*r/d) > 1) return false;
        tangent = norm(theta+(cw1 ? 1 : -1)*std::acos(2*r/d));
        // cf2b..d063: length from the point two radii along this radial to C2.
        double lx = b.cx-(a.cx+2*r*std::cos(tangent));
        double lz = b.cz-(a.cz+2*r*std::sin(tangent));
        line.length = std::sqrt(lx*lx+lz*lz);
    }
    a.sweep = norm(cw1 ? a.angle-tangent : tangent-a.angle); a.length = a.sweep*r;
    b.angle = norm(tangent+(cw1 == cw2 ? 0 : pi));
    b.sweep = norm(cw2 ? b.angle-endRadial : endRadial-b.angle); b.length = b.sweep*r;
    line.x = a.cx+std::cos(tangent)*r; line.z = a.cz+std::sin(tangent)*r;
    line.angle = norm(tangent+(cw1 ? -pi/2 : pi/2));
    p.end = end;
    return true;
}

// 0x42cd70: directions (CW,CW), (CW,CCW), (CCW,CW), (CCW,CCW).
bool turnToHeading(const Node& start, const Node& end, double radius, Path& out)
{
    if (radius <= 0) return false;
    double best = 9999999; bool found = false;
    for (int i = 0; i < 4; ++i) {
        Path p;
        if (arcLineArc(start,end,i < 2,i == 0 || i == 2,radius,p) && p.length() < best) {
            best = p.length(); out = p; found = true;
        }
    }
    return found;
}

bool connect(const Node& start, const Node& end, double radius, Path& out)
{
    return end.heading < 0 ? turnToPoint(start,end,radius,out) : turnToHeading(start,end,radius,out);
}

// 0x42cd37/0x42ce86 return the last positive-length piece index. 0x42c450
// copies its +30 verbatim, even when a zero tangent leaves an arc last.
static double routeHeading(const Path& p)
{
    for (int i = 3; i >= 0; --i) if (p.pieces[i].length > 0) return p.pieces[i].angle;
    return p.pieces[0].angle;
}

// 0x42c920: sample [0,total), centre plus four corners of the clearance square.
// 0x442cfc truncates towards zero; it is not floor (notably outside the grid).
bool clear(const Path& path, const Grid& grid)
{
    if (grid.step <= 0) return false; // port: prevent an invalid sampling loop.
    for (double d = 0; d < path.length(); d += grid.step) {
        Node p = path.at(d); double half = grid.width*0.5;
        int hiX = (int)(p.x+half), hiZ = (int)(p.z+half);
        int loX = (int)(p.x-half), loZ = (int)(p.z-half);
        if ((loX < 0 || loZ < 0 || hiX >= 30 || hiZ >= 30) && !grid.allowOutside) return false;
        int xs[5] = {(int)p.x,hiX,loX,hiX,loX};
        int zs[5] = {(int)p.z,hiZ,hiZ,loZ,loZ};
        for (int k = 0; k < 5; ++k)
            if (xs[k] >= 0 && xs[k] < 30 && zs[k] >= 0 && zs[k] < 30 && grid.blocked[xs[k]][zs[k]]) return false;
    }
    return true;
}

// 0x42c450: reverse the search's destination-first records into a linked list,
// try skipping its current node, remove on a clear candidate; otherwise commit
// that node as the new anchor. Preserve the chosen arrival heading on free nodes.
void round(std::vector<Node>& nodes, const Node& start, const Grid& grid, double radius)
{
    if (nodes.empty()) return;
    Path p;
    if (nodes[0].heading < 0 && turnToPoint(start,nodes[0],radius,p)) nodes[0].heading = routeHeading(p);
    Node anchor = start;
    std::size_t current = 0;
    while (current+1 < nodes.size()) {
        Node& next = nodes[current+1];
        if (connect(anchor,next,radius,p) && clear(p,grid)) {
            if (next.heading < 0) next.heading = routeHeading(p);
            nodes.erase(nodes.begin()+current);
        } else {
            if (next.heading < 0 && turnToPoint(nodes[current],next,radius,p)) next.heading = routeHeading(p);
            anchor = nodes[current++];
        }
    }
    // port: keep walking order here; the exe reverses back for its stack consumer.
}
}
