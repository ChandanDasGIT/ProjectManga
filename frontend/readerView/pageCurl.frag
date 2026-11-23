#version 440

// A page rolled up around a cylinder lying on the fold line. Beyond the fold the paper wraps the
// cylinder (front side on the way up, back side over the top) and then lies flat on top, back
// side up. Where it has lifted away, the page underneath shows with the curl's shadow on it.
// Everything is in this item's pixels; the output is premultiplied.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 areaSize;
    vec4 pageRect;      // the turning page laid flat: x, y, width, height
    vec2 foldPoint;
    vec2 foldNormal;    // unit, pointing to the paper's free edge
    float radius;
    float hasBack;      // 0: the back of the paper is blank
    vec4 paper;
};

layout(binding = 1) uniform sampler2D front;
layout(binding = 2) uniform sampler2D back;

const float PI = 3.14159265;

vec2 uvOf(vec2 p) { return (p - pageRect.xy) / pageRect.zw; }

// 1 inside the page, 0 outside, a pixel of soft edge between.
float coverage(vec2 p)
{
    vec2 a = p - pageRect.xy;
    vec2 b = pageRect.xy + pageRect.zw - p;
    return clamp(min(min(a.x, a.y), min(b.x, b.y)) + 0.5, 0.0, 1.0);
}

float distanceOutside(vec2 p)
{
    vec2 q = abs(p - (pageRect.xy + pageRect.zw * 0.5)) - pageRect.zw * 0.5;
    return length(max(q, 0.0));
}

vec3 frontAt(vec2 p) { return texture(front, uvOf(p)).rgb; }

vec3 backAt(vec2 p)
{
    vec2 uv = uvOf(p);
    return hasBack > 0.5 ? texture(back, vec2(1.0 - uv.x, uv.y)).rgb : paper.rgb;
}

// `layer` (premultiplied) laid over `under`.
vec4 over(vec4 layer, vec4 under) { return layer + under * (1.0 - layer.a); }

void main()
{
    vec2 p = qt_TexCoord0 * areaSize;
    vec2 n = foldNormal;
    float r = max(radius, 0.001);
    float d = dot(p - foldPoint, n);

    vec4 col = vec4(0.0);
    if (d < 0.0) {
        // Still flat, a little darker next to the fold.
        float dark = 0.16 * exp(d / (r * 1.5 + 6.0));
        col = vec4(frontAt(p) * (1.0 - dark), 1.0) * coverage(p);
    } else {
        // Lifted away: the page underneath shows, shaded near the curl.
        float shadow = 0.5 * (1.0 - smoothstep(0.0, r * 1.5 + 30.0, d - r * 0.6));
        col = vec4(0.0, 0.0, 0.0, shadow * coverage(p));
    }

    vec2 top = vec2(-1.0e5);
    float topLight = 1.0;
    if (d >= 0.0 && d <= r) {
        float th = asin(d / r);
        // Front side, on the way up the roll.
        vec2 src = p + (r * th - d) * n;
        float light = 1.0 - 0.45 * pow(sin(th), 2.0);
        col = over(vec4(frontAt(src) * light, 1.0) * coverage(src), col);
        // Back side, over the top of the roll, with a sheen where it faces the light.
        top = p + (r * (PI - th) - d) * n;
        float x = d / r;
        topLight = 0.72 + 0.24 * (1.0 - x) + 0.1 * exp(-pow((x - 0.35) * 4.0, 2.0));
    } else if (d < 0.0) {
        // Back side, lying over the flat paper.
        top = p + (PI * r - 2.0 * d) * n;
        topLight = 0.88 + 0.1 * smoothstep(0.0, 2.0 * r + 12.0, -d);
        // Its shadow around its edges.
        float shadow = 0.38 * (1.0 - smoothstep(0.0, 24.0, distanceOutside(top)));
        col = over(vec4(0.0, 0.0, 0.0, shadow), col);
    }
    col = over(vec4(backAt(top) * topLight, 1.0) * coverage(top), col);

    fragColor = col * qt_Opacity;
}
