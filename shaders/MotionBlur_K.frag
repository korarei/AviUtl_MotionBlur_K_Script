#version 460 core

in vec2 TexCoord;

layout(location = 0) out vec4 FragColor;

uniform sampler2D texture0;
uniform vec2 resolution;
uniform vec2 pivot;
uniform int is_orig_img_visible;
uniform ivec2 samples;

uniform vec2 step_pos_offset;
uniform float step_scale_offset;
uniform mat2 step_rot_mat_offset;

uniform vec2 step_pos_seg1;
uniform float step_scale_seg1;
uniform mat2 step_rot_mat_seg1;

uniform vec2 step_pos_seg2;
uniform float step_scale_seg2;
uniform mat2 step_rot_mat_seg2;


// Clamp the texture coordinates to avoid sampling outside the texture bounds.
vec4
safe_texture(in sampler2D tex, in vec2 uv, in vec2 resolution) {
    if (uv.x < 0.0 || uv.x > resolution.x || uv.y < 0.0 || uv.y > resolution.y) {
        return vec4(0.0);
    }
    return texture(tex, uv / resolution);
}

// Blur the texture using the given parameters.
int
blur(inout vec2 uv, inout vec4 color, in int samples, in vec2 step_pos, in float step_scale, in mat2 step_rot_mat) {
    int sample_count = 0;

    vec2 localized_step_pos = step_pos;
    for (int i = 1; i <= samples; i++) {
        uv -= localized_step_pos;
        uv *= step_rot_mat / step_scale;

        vec4 step_color = safe_texture(texture0, uv + pivot, resolution);
        step_color.rgb *= step_color.a;
        color += step_color;

        sample_count++;
        localized_step_pos *= step_rot_mat / step_scale;
    }

    return sample_count;
}

// Guessed AviUtl's normal blend.
vec4
blend(in vec4 color1, in vec4 color2) {
    vec4 result;

    float t = 1.0 - color2.a;
    float w1 = color1.a * t;
    float w2 = color2.a * color2.a;
    float denominator = w1 + w2;
    float is_zero = step(denominator, 0.0);
    vec3 blend_result = (color1.rgb * w1 + color2.rgb * w2) / max(denominator, 0.0001);
    result.rgb = mix(blend_result, vec3(0.0), is_zero);

    result.a = color1.a + color2.a * (1.0 - color1.a);

    return clamp(result, 0.0, 1.0);
}

void
main() {
    vec2 uv = TexCoord * resolution - pivot;
    uv *= step_scale_offset;
    uv += step_pos_offset;
    uv = step_rot_mat_offset * uv;
    vec4 color = safe_texture(texture0, uv + pivot, resolution);
    color.rgb *= color.a;

    int sample_count = 1;

    sample_count += blur(uv, color, samples.x, step_pos_seg1, step_scale_seg1, step_rot_mat_seg1);
    if (samples.y != 0) {
        sample_count += blur(uv, color, samples.y, step_pos_seg2, step_scale_seg2, step_rot_mat_seg2);
    }

    // Avoid division by zero.
    // Division by zero can occur when the sample count is small, and the blur width is large.
    // When division by zero occurs, the blend function (alpha mix) used later for blending the original image can't operate correctly.
    // This is likely because the resulting value becomes infinity.
    float is_zero = step(color.a, 0.0);
    color.rgb = mix(color.rgb / max(color.a, 0.0001), vec3(0.0), is_zero); // 
    color.a /= sample_count;
    color = clamp(color, 0.0, 1.0);
    // Blend the original image with the blurred image if is_orig_img_visible is true.
    if (bool(is_orig_img_visible)) {
        color = blend(color, texture(texture0, TexCoord));
    }
    FragColor = color;
}