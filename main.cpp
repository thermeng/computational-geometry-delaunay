#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <map>

// Point structure
struct Point {
    double x, y;

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
    // Add a less-than operator to use Point as a map key
    bool operator<(const Point& other) const {
        if (x < other.x) return true;
        if (x > other.x) return false;
        return y < other.y;
    }
};

// Edge structure
struct Edge {
    Point p1, p2;

    bool operator==(const Edge& other) const {
        return (p1 == other.p1 && p2 == other.p2) || (p1 == other.p2 && p2 == other.p1);
    }
};

// Triangle structure
struct Triangle {
    Point a, b, c;
};

// Function to check if point p is inside the circumcircle of triangle t
bool inCircumcircle(const Point& p, const Triangle& t) {
    double ax = t.a.x - p.x;
    double ay = t.a.y - p.y;
    double bx = t.b.x - p.x;
    double by = t.b.y - p.y;
    double cx = t.c.x - p.x;
    double cy = t.c.y - p.y;

    double det = (ax * ax + ay * ay) * (bx * cy - cx * by) -
                 (bx * bx + by * by) * (ax * cy - cx * ay) +
                 (cx * cx + cy * cy) * (ax * by - bx * ay);

    // For a counter-clockwise triangle, the point is inside if det > 0
    // We need to ensure triangles are consistently oriented (e.g., CCW)
    // but for Bowyer-Watson, the sign consistency is what matters.
    return det > 1e-9;
}

// Delaunay triangulation function
std::vector<Triangle> delaunayTriangulation(std::vector<Point>& points) {
    std::vector<Triangle> triangles;

    // Determine the bounds of the points
    double minX = points[0].x;
    double minY = points[0].y;
    double maxX = minX;
    double maxY = minY;
    for (const auto& p : points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }
    double dx = maxX - minX;
    double dy = maxY - minY;
    double deltaMax = std::max(dx, dy);
    double midX = (minX + maxX) / 2.0;
    double midY = (minY + maxY) / 2.0;

    // Create a super triangle that encompasses all points
    Point p1 = {midX - 20 * deltaMax, midY - deltaMax};
    Point p2 = {midX + 20 * deltaMax, midY - deltaMax};
    Point p3 = {midX, midY + 20 * deltaMax};
    Triangle superTriangle = {p1, p2, p3};
    triangles.push_back(superTriangle);

    for (const auto& point : points) {
        std::vector<Triangle> badTriangles;
        std::vector<Edge> polygon;

        // Find triangles whose circumcircle contains the point
        for (const auto& triangle : triangles) {
            if (inCircumcircle(point, triangle)) {
                badTriangles.push_back(triangle);
                polygon.push_back({triangle.a, triangle.b});
                polygon.push_back({triangle.b, triangle.c});
                polygon.push_back({triangle.c, triangle.a});
            }
        }

        // Remove bad triangles from the triangulation
        triangles.erase(std::remove_if(triangles.begin(), triangles.end(),
            [&](const Triangle& t) {
                for (const auto& bad : badTriangles) {
                    if (t.a == bad.a && t.b == bad.b && t.c == bad.c) {
                        return true;
                    }
                }
                return false;
            }), triangles.end());

        // Find the unique edges of the polygonal hole
        std::vector<Edge> uniqueEdges;
        for (size_t i = 0; i < polygon.size(); ++i) {
            bool isUnique = true;
            for (size_t j = 0; j < polygon.size(); ++j) {
                if (i != j && polygon[i] == polygon[j]) {
                    isUnique = false;
                    break;
                }
            }
            if (isUnique) {
                uniqueEdges.push_back(polygon[i]);
            }
        }

        // Create new triangles from the point to the unique edges
        for (const auto& edge : uniqueEdges) {
            triangles.push_back({edge.p1, edge.p2, point});
        }
    }

    // Remove triangles that share a vertex with the super triangle
    triangles.erase(std::remove_if(triangles.begin(), triangles.end(),
        [&](const Triangle& t) {
            return t.a == superTriangle.a || t.b == superTriangle.a || t.c == superTriangle.a ||
                   t.a == superTriangle.b || t.b == superTriangle.b || t.c == superTriangle.b ||
                   t.a == superTriangle.c || t.b == superTriangle.c || t.c == superTriangle.c;
        }), triangles.end());

    return triangles;
}

// Function to export triangles to a VTK file
void exportToVTK(const std::vector<Triangle>& triangles, const std::string& filename) {
    std::ofstream vtkFile(filename);
    if (!vtkFile.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    vtkFile << "# vtk DataFile Version 3.0\n";
    vtkFile << "Delaunay Triangulation\n";
    vtkFile << "ASCII\n";
    vtkFile << "DATASET UNSTRUCTURED_GRID\n";

    // Collect unique points and map them to an index
    std::map<Point, int> pointMap;
    int pointIndex = 0;
    for (const auto& tri : triangles) {
        if (pointMap.find(tri.a) == pointMap.end()) pointMap[tri.a] = pointIndex++;
        if (pointMap.find(tri.b) == pointMap.end()) pointMap[tri.b] = pointIndex++;
        if (pointMap.find(tri.c) == pointMap.end()) pointMap[tri.c] = pointIndex++;
    }

    // Write unique points
    vtkFile << "POINTS " << pointMap.size() << " float\n";
    std::vector<Point> uniquePoints(pointMap.size());
    for(const auto& pair : pointMap) {
        uniquePoints[pair.second] = pair.first;
    }
    for(const auto& p : uniquePoints) {
        vtkFile << p.x << " " << p.y << " 0.0\n";
    }

    // Write triangles (cells)
    vtkFile << "CELLS " << triangles.size() << " " << triangles.size() * 4 << "\n";
    for (const auto& tri : triangles) {
        vtkFile << "3 " << pointMap[tri.a] << " " << pointMap[tri.b] << " " << pointMap[tri.c] << "\n";
    }

    // Write cell types
    vtkFile << "CELL_TYPES " << triangles.size() << "\n";
    for (size_t i = 0; i < triangles.size(); ++i) {
        vtkFile << "5\n"; // VTK_TRIANGLE
    }

    vtkFile.close();
    std::cout << "Exported to " << filename << std::endl;
}

int main() {
    std::vector<Point> points = {
        {0.0, 0.0}, {0.7, 1.4}, {2.7, 2.7}, {6.0, 3.8},
        {10.5, 4.8}, {16.1, 5.5}, {22.7, 5.9}, {29.9, 6.0},
        {37.7, 5.9}, {45.9, 5.5}, {54.1, 5.0}, {62.3, 4.4},
        {70.1, 3.6}, {77.3, 2.9}, {83.9, 2.1}, {89.5, 1.4},
        {94.0, 0.8}, {97.3, 0.4}, {99.3, 0.1}, {0.7, -1.4},
        {2.7, -2.7}, {6.0, -3.8}, {10.5, -4.8}, {16.1, -5.5},
        {22.7, -5.9}, {29.9, -6.0}, {37.7, -5.9}, {45.9, -5.5},
        {54.1, -5.0}, {62.3, -4.4}, {70.1, -3.6}, {77.3, -2.9},
        {83.9, -2.1}, {89.5, -1.4}, {94.0, -0.8}, {97.3, -0.4},
        {99.3, -0.1}, {0.7, 0.0}, {2.7, 0.0}, {6.0, 0.0},
        {10.5, 0.0}, {16.1, 0.0}, {22.7, 0.0}, {29.9, 0.0},
        {37.7, 0.0}, {45.9, 0.0}, {54.1, 0.0}, {62.3, 0.0},
        {70.1, 0.0}, {77.3, 0.0}, {83.9, 0.0}, {89.5, 0.0},
        {94.0, 0.0}, {97.3, 0.0}, {99.3, 0.0}, {100.0, 0.0}
    };   

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<Triangle> triangles = delaunayTriangulation(points);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> duration = end - start;
    std::cout << "Time taken for triangulation: " << duration.count() << " seconds." << std::endl;
    std::cout << "Generated " << triangles.size() << " triangles." << std::endl;

    // Export triangles to VTK file
    exportToVTK(triangles, "triangulation.vtk");

    return 0;
}

