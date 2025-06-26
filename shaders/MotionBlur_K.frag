#version 460 core

in vec2 TexCoord;

layout(location = 0) out vec4 FragColor;

uniform sampler2D texture0;
uniform vec2 resolution;
uniform vec2 pivot;
uniform int mix_orig_img;
uniform ivec2 samp;

uniform vec2 step_pos_offset;
uniform float step_scale_offset;
uniform mat2 step_rot_mat_offset;

uniform mat3 htm_init_seg1;
uniform mat3 adj_seg1;

uniform mat3 htm_init_seg2;
uniform mat3 adj_seg2;


// Clamp the texture coordinates to avoid sampling outside the texture bounds.
vec4
safe_texture(in vec2 uv) {
    if (uv.x < 0.0 || uv.x > resolution.x || uv.y < 0.0 || uv.y > resolution.y) {
        return vec4(0.0);
    }
    return texture(texture0, uv / resolution);
}

// Blur the texture using the given parameters.
int
blur(inout vec3 uv3, inout vec4 col, in int samp, in mat3 htm_init, in mat3 adj) {
    int samp_cnt = 0;

    mat3 htm = htm_init;
    for (int i = 1; i <= samp; ++i) {
        uv3 = htm * uv3;

        vec4 step_col = safe_texture(uv3.st + pivot);
        step_col.rgb *= step_col.a;
        col += step_col;

        htm[2] = adj * htm[2];
        samp_cnt++;
    }

    return samp_cnt;
}

// Guessed AviUtl's normal blend.
vec4
blend(in vec4 col1, in vec4 col2) {
    vec4 result;

    float t = 1.0 - col2.a;
    float w1 = col1.a * t;
    float w2 = col2.a * col2.a;
    float denominator = w1 + w2;
    float is_zero = step(denominator, 0.0);
    vec3 blend_result = (col1.rgb * w1 + col2.rgb * w2) / max(denominator, 1.0e-4);
    result.rgb = mix(blend_result, vec3(0.0), is_zero);

    result.a = col1.a + col2.a * (1.0 - col1.a);

    return clamp(result, 0.0, 1.0);
}

void
main() {
    vec2 uv = TexCoord * resolution - pivot;
    uv *= step_scale_offset;
    uv += step_pos_offset;
    uv = step_rot_mat_offset * uv;
    vec4 col = safe_texture(uv + pivot);
    col.rgb *= col.a;

    int samp_cnt = 1;
    vec3 uv3 = vec3(uv, 1.0);
    samp_cnt += blur(uv3, col, samp[0], htm_init_seg1, adj_seg1);
    if (samp[1] != 0) {
        samp_cnt += blur(uv3, col, samp[1], htm_init_seg2, adj_seg2);
    }
    uv = uv3.st;

    // Avoid division by zero.
    // Division by zero can occur when the sample count is small, and the blur width is large.
    // When division by zero occurs, the blend function (alpha mix) used later for blending the original image can't operate correctly.
    // This is likely because the resulting value becomes infinity.
    float is_zero = step(col.a, 0.0);
    col.rgb = mix(col.rgb / max(col.a, 1.0e-4), vec3(0.0), is_zero); // 
    col.a /= samp_cnt;
    col = clamp(col, 0.0, 1.0);
    // Blend the original image with the blurred image if is_orig_img_visible is true.
    if (bool(mix_orig_img)) {
        col = blend(col, texture(texture0, TexCoord));
    }
    FragColor = col;
}