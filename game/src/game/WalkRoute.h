// sb2.exe 0x42c450 and its circular-turn helpers. All coordinates are in cells.
#pragma once
#include <vector>

namespace sbwalk {

// Retail route record, 0x18 bytes: +00 double x, +08 double z,
// +10 double tangent heading (radians; -1 means not yet constrained).
// 0x42c450 temporarily adds +18 next / +1c previous (32-bit pointers).
struct Node {
    double x, z, heading;
    Node(double px = 0, double pz = 0, double h = -1) : x(px), z(pz), heading(h) {}
};

// Retail motion piece, 0x48 bytes (0x42cbb0/0x42d5f0/0x42d870):
// +00 u8 arc, +01..07 padding; +08/+10 double circle centre x/z;
// +18 u8 clockwise, +19..1f padding; +20/+28 double line start x/z;
// +30 double angle (arc's initial radial angle, or line tangent heading);
// +38 double positive arc sweep; +40 double distance (radius*sweep for arcs).
// A candidate contains four pieces, 0x120 bytes. Radius lives at solver+390.
struct Piece {
    unsigned char arc, pad0[7];
    double cx, cz;
    unsigned char clockwise, pad1[7];
    double x, z, angle, sweep, length;
    Piece();
};

struct Path {
    Piece pieces[4];
    double radius;
    Node end; // port: explicit endpoint for stable evaluation at/beyond total length.
    Path() : radius(0.1) {}
    double length() const;
    Node at(double distance) const; // 0x42dc90
};

struct Grid {
    bool blocked[30][30];
    bool allowOutside;
    double width, step;
    Grid(); // 0x42a670 defaults: width .1, step .2, allow outside.
};

Path straight(const Node& start, const Node& end); // 0x42cbb0
bool turnToPoint(const Node& start, const Node& end, double radius, Path& out); // 0x42cc50
bool turnToHeading(const Node& start, const Node& end, double radius, Path& out); // 0x42cd70
bool clear(const Path& path, const Grid& grid); // 0x42c920
// 0x42c450: input/output in walking order (port: vectors replace reversed linked list).
void round(std::vector<Node>& nodes, const Node& start, const Grid& grid, double radius = 0.1);
bool connect(const Node& start, const Node& end, double radius, Path& out);

}
