#pragma once

#include <math.h>

class vec3 {
public:
    float x;
    float y;
    float z;

    vec3(float x, float y, float z) {
        this->x = x;
        this->y = y;
        this->z = z;
    }

    vec3() {
        this->x = this->y = this->z = 0;
    }

    friend vec3 operator-(const vec3& a, const vec3& b) {
        return vec3(
            a.x - b.x,
            a.y - b.y,
            a.z - b.z
            );
    }

    static float length(const vec3& a) {
        return sqrt(vec3::dot(a, a));
    }

    static float dot(const vec3& a, const vec3& b) {
        return a.x*b.x + a.y*b.y + a.z*b.z;
    }

    static float distance(const vec3& a, const vec3& b) {
        return length(a - b);
    }
};

class vec2 {
public:
    vec2(float x, float y) {
        this->x = x;
        this->y = y;
    }

    float x;
    float y;
};

class Tri {
public:
    int i[3];

    Tri (int i0, int i1, int i2){
        i[0] = i0;
        i[1] = i1;
        i[2] = i2;
    }

    Tri (){
        i[0] = 0;
        i[1] = 0;
        i[2] = 0;
    }
};
