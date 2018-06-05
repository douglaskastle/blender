#ifdef OB_TEXTURE
uniform sampler2D image;
#endif
uniform mat3 normalWorldMatrix;
uniform float alpha = 0.5;

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
in vec3 normal_viewport;
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */
#ifdef OB_TEXTURE
in vec2 uv_interp;
#endif

layout(std140) uniform world_block {
	WorldData world_data;
};

layout(std140) uniform material_block {
	MaterialData material_data;
};

layout(location=0) out vec4 transparentAccum;


void main()
{
	vec4 diffuse_color;
#ifdef OB_SOLID
	diffuse_color = material_data.diffuse_color;
#endif /* OB_SOLID */
#ifdef OB_TEXTURE
	diffuse_color = texture(image, uv_interp);
#endif /* OB_TEXTURE */

#ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
	vec3 specular_color = get_world_specular_lights(world_data, vec4(material_data.specular_color.rgb, material_data.roughness), normal_viewport, vec3(0.0, 0.0, 1.0));
#else
	vec3 specular_color = vec3(0.0);
#endif

#ifdef V3D_LIGHTING_STUDIO
#  ifdef STUDIOLIGHT_ORIENTATION_CAMERA
	vec3 diffuse_light = get_camera_diffuse_light(world_data, normal_viewport);
#  endif
#  ifdef STUDIOLIGHT_ORIENTATION_WORLD
	vec3 normal_world = normalWorldMatrix * normal_viewport;
	vec3 diffuse_light = get_world_diffuse_light(world_data, normal_world);
#  endif
	vec3 shaded_color = diffuse_light * diffuse_color.rgb + specular_color;
#else
	vec3 shaded_color = diffuse_color.rgb + specular_color;
#endif

	vec4 premultiplied = vec4(shaded_color.rgb * alpha, alpha);
	transparentAccum = calculate_transparent_accum(premultiplied);
}
