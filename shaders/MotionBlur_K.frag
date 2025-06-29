#version 460 core

in vec2 TexCoord;

layout(location = 0) out vec4 FragColor;

uniform sampler2D texture0;
uniform vec2 res;
uniform vec2 pivot;
uniform int mix_orig_img;
uniform ivec2 samp;

uniform mat3 htm_offset;
uniform mat3 init_htm_seg1;
uniform mat3 init_htm_seg2;

const float EPSILON = 1.0e-6;

// Clamp the texture coordinates to avoid sampling outside the texture bounds.
vec4
safe_texture(in vec2 uv) {
    vec2 coord = uv / res;
    vec2 is_inside = step(vec2(0.0), coord) * step(coord, vec2(1.0));
    float mask = is_inside.x * is_inside.y;
    return mix(vec4(0.0), texture(texture0, coord), mask);
}

// Blur the texture using the given parameters.
void
apply_blur(inout vec3 uv3, inout vec4 col, in int samp, in mat3 init_htm) {
    mat3 htm = init_htm;
    mat3 adj_mat = init_htm;
    adj_mat[2] = vec3(0.0, 0.0, 1.0);
    
    for (int i = 1; i <= samp; ++i) {
        uv3 = htm * uv3;

        vec4 samp_col = safe_texture(uv3.st + pivot);
        samp_col.rgb *= samp_col.a;
        col += samp_col;

        htm[2] = adj_mat * htm[2];
    }
}

// Guessed AviUtl's normal blend.
vec4
blend(in vec4 col_base, in vec4 col_blend) {
    vec4 result;

    float w_base = col_base.a * (1.0 - col_blend.a);
    float w_blend = col_blend.a * col_blend.a;
    float total_w = w_base + w_blend;

    float is_zero = step(total_w, EPSILON);
    vec3 mixed_rgb = (col_base.rgb * w_base + col_blend.rgb * w_blend) / max(total_w, EPSILON);
    result.rgb = mix(mixed_rgb, vec3(0.0), is_zero);
    result.a = col_base.a + col_blend.a * (1.0 - col_base.a);

    return clamp(result, 0.0, 1.0);
}

void
main() {
    vec2 uv = TexCoord * res - pivot;
    vec3 uv3 = vec3(uv, 1.0);

    // Set the start position.
    uv3 = htm_offset * uv3;
    vec4 col = safe_texture(uv3.st + pivot);
    col.rgb *= col.a;

    int total_samp = 1;

    // Apply blur.
    apply_blur(uv3, col, samp[0], init_htm_seg1);
    total_samp += samp[0];

    if (samp[1] != 0) {
        apply_blur(uv3, col, samp[1], init_htm_seg2);
        total_samp += samp[1];
    }

    // Compute weighted average color with alpha, avoiding division by zero.
    float is_zero = step(col.a, EPSILON);
    col.rgb = mix(col.rgb / max(col.a, EPSILON), vec3(0.0), is_zero);
    col.a /= float(total_samp);
    col = clamp(col, 0.0, 1.0);

    // Blend the original image with the blurred image.
    if (bool(mix_orig_img)) {
        col = blend(col, texture(texture0, TexCoord));
    }

    FragColor = col;
}