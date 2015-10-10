#version 150
#extension GL_ARB_shading_language_420pack : require
#extension GL_ARB_explicit_attrib_location : require

#define TASK 21  // 21 22 31 32 33 4 5
#define ENABLE_OPACITY_CORRECTION 0
#define ENABLE_LIGHTNING 0
#define ENABLE_SHADOWING 0
#define ENABLE_VOL0 1
#define ENABLE_VOL2 0
#define ENABLE_VOL3 0
#define ENABLE_VOL4 0
#define PAIR 24
#define FUNC 1

in vec3 ray_entry_position;

layout(location = 0) out vec4 FragColor;

uniform mat4 Modelview;

uniform sampler3D volume_texture0;
uniform sampler3D volume_texture2;
uniform sampler3D volume_texture3;
uniform sampler3D volume_texture4;

uniform sampler2D transfer_texture;
uniform sampler2D transfer_texture2;
uniform sampler2D transfer_texture3;
uniform sampler2D transfer_texture4;

uniform sampler2D multi_transfer1;
uniform sampler2D multi_transfer2;
uniform sampler2D multi_transfer3;
uniform sampler2D multi_transfer4;


uniform vec3    camera_location;
uniform float   sampling_distance;
uniform float   sampling_distance_ref;
uniform float   iso_value;
uniform vec3    max_bounds;
uniform ivec3   volume_dimensions;

uniform vec3    light_position;
uniform vec3    light_ambient_color;
uniform vec3    light_diffuse_color;
uniform vec3    light_specular_color;
uniform float   light_ref_coef;


bool
inside_volume_bounds(const in vec3 sampling_position)
{
	return (all(greaterThanEqual(sampling_position, vec3(0.0)))
		&& all(lessThanEqual(sampling_position, max_bounds)));
}


float
get_sample_data(vec3 in_sampling_pos){

	vec3 obj_to_tex = vec3(1.0) / max_bounds;
	return texture(volume_texture0, in_sampling_pos * obj_to_tex).r;
	// r- 1 channel, rgb - 3 channel
}

vec3
get_sample_data0(vec3 in_sampling_pos){
	vec3 obj_to_tex = vec3(1.0) / max_bounds;
	return texture(volume_texture0, in_sampling_pos * obj_to_tex).rgb;

}

vec3
get_sample_data2(vec3 in_sampling_pos){
	vec3 obj_to_tex = vec3(1.0) / max_bounds;
	return texture(volume_texture2, in_sampling_pos * obj_to_tex).rgb;

}

float
get_sample_data3(vec3 in_sampling_pos){

	vec3 obj_to_tex = vec3(1.0) / max_bounds;
	return texture(volume_texture3, in_sampling_pos * obj_to_tex).r;
}

float
get_sample_data4(vec3 in_sampling_pos){

	vec3 obj_to_tex = vec3(1.0) / max_bounds;
	return texture(volume_texture4, in_sampling_pos * obj_to_tex).r;
}


vec3
get_gradient(vec3 in_sampling_pos){
	float x = in_sampling_pos.x;
	float y = in_sampling_pos.y;
	float z = in_sampling_pos.z;


	float dx = (get_sample_data(vec3(x + 1.0 / volume_dimensions.x, y, z)) - get_sample_data(vec3(x - 1.0 / volume_dimensions.x, y, z))) * 0.5;
	float dy = (get_sample_data(vec3(x, y + 1.0 / volume_dimensions.y, z)) - get_sample_data(vec3(x, y - 1.0 / volume_dimensions.y, z))) * 0.5;
	float dz = (get_sample_data(vec3(x, y, z + 1.0 / volume_dimensions.z)) - get_sample_data(vec3(x, y, z - 1.0 / volume_dimensions.z))) * 0.5;
	return vec3(dx, dy, dz);

}

void main()
{
	/// One step trough the volume
	vec3 ray_increment = normalize(ray_entry_position - camera_location) * sampling_distance;
	/// Position in Volume
	vec3 sampling_pos = ray_entry_position + ray_increment; // test, increment just to be sure we are in the volume

	/// Init color of fragment
	vec4 dst = vec4(0.0, 0.0, 0.0, 0.0);

	/// check if we are inside volume
	bool inside_volume = inside_volume_bounds(sampling_pos);

#if TASK == 21 // ASSIGNMENT 1
	vec4 max_val = vec4(0.0, 0.0, 0.0, 0.0);
	//vec3 inten = vec3(0.0, 0.0, 0.0);
	vec4 inten = vec4(0.0, 0.0, 0.0, 0.0);

	vec4 max_val0 = vec4(0.0, 0.0, 0.0, 0.0);
	vec4 max_val2 = vec4(0.0, 0.0, 0.0, 0.0);
	vec4 max_val3 = vec4(0.0, 0.0, 0.0, 0.0);
	vec4 max_val4 = vec4(0.0, 0.0, 0.0, 0.0);

	// the traversal loop,
	// termination when the sampling position is outside volume boundarys
	// another termination condition for early ray termination is added
	while (inside_volume)
	{
		// get sample
		float s = get_sample_data(sampling_pos);

		vec3 s0 = get_sample_data0(sampling_pos);
		vec3 s2 = get_sample_data2(sampling_pos);
		float s3 = get_sample_data3(sampling_pos);
		float s4 = get_sample_data4(sampling_pos);
		float s00 = sqrt(pow(s0.x, 2) + pow(s0.y, 2) + pow(s0.z, 2));
		float s20 = sqrt(pow(s2.x, 2) + pow(s2.y, 2) + pow(s2.z, 2));


		// apply the transfer functions to retrieve color and opacity
		vec4 color = texture(transfer_texture, vec2(s, s)); // This is where classification happens

		vec4 color01 = texture(transfer_texture, vec2(s0.x, s0.x));
		vec4 color02 = texture(transfer_texture, vec2(s0.y, s0.y));
		vec4 color03 = texture(transfer_texture, vec2(s0.z, s0.z));
		vec4 color21 = texture(transfer_texture, vec2(s2.x, s2.x));
		vec4 color22 = texture(transfer_texture, vec2(s2.x, s2.x));
		vec4 color23 = texture(transfer_texture, vec2(s2.x, s2.x));

		vec4 color0 = texture(transfer_texture, vec2(s00, s00));
		vec4 color2 = texture(transfer_texture2, vec2(s20, s20));
		vec4 color3 = texture(transfer_texture3, vec2(s3, s3));
		vec4 color4 = texture(transfer_texture4, vec2(s4, s4));

		// this is the example for maximum intensity projection
		max_val0.r = max(color0.r, max_val0.r);
		max_val0.g = max(color0.g, max_val0.g);
		max_val0.b = max(color0.b, max_val0.b);
		max_val0.a = max(color0.a, max_val0.a);

		max_val2.r = max(color2.r, max_val2.r);
		max_val2.g = max(color2.g, max_val2.g);
		max_val2.b = max(color2.b, max_val2.b);
		max_val2.a = max(color2.a, max_val2.a);

		max_val3.r = max(color3.r, max_val3.r);
		max_val3.g = max(color3.g, max_val3.g);
		max_val3.b = max(color3.b, max_val3.b);
		max_val3.a = max(color3.a, max_val3.a);

		max_val4.r = max(color4.r, max_val4.r);
		max_val4.g = max(color4.g, max_val4.g);
		max_val4.b = max(color4.b, max_val4.b);
		max_val4.a = max(color4.a, max_val4.a);


		//max_val.r = max(color.r, max_val.r);
		//max_val.g = max(color.g, max_val.g);
		//max_val.b = max(color.b, max_val.b);
		//max_val.a = max(color.a, max_val.a);

		// increment the ray sampling position
		sampling_pos += ray_increment;

		// update the loop termination condition
		inside_volume = inside_volume_bounds(sampling_pos);
	}

#if ENABLE_VOL0 == 1	

	//inten = inten + trans*total.rgb*color.a;
	//inten = max_val0.a*max_val0.rgb;
	inten = inten + max_val0;
#endif
#if ENABLE_VOL2 == 1

	//inten = inten + max_val2.a*max_val2.rgb;
	inten  = inten + max_val2;
#endif
#if ENABLE_VOL3 == 1

	//inten = inten + max_val3.a*max_val3.rgb;
	inten = inten + max_val3;
#endif
#if ENABLE_VOL4 == 1

	//inten = inten + max_val4.a*max_val4.rgb;
	inten = inten + max_val4;
#endif



	dst = inten;
	//dst = vec4(inten, 1.0);
#endif 

#if TASK == 22 // ASSIGNMENT 1

	// the traversal loop,
	// termination when the sampling position is outside volume boundarys
	// another termination condition for early ray termination is added
	int count = 0;
	vec4 average_val = vec4(0.0, 0.0, 0.0, 0.0);

	vec4 average_val0 = vec4(0.0, 0.0, 0.0, 0.0);
	vec4 average_val2 = vec4(0.0, 0.0, 0.0, 0.0);
	vec4 average_val3 = vec4(0.0, 0.0, 0.0, 0.0);
	vec4 average_val4 = vec4(0.0, 0.0, 0.0, 0.0);

	vec4 inten = vec4(0.0, 0.0, 0.0, 0.0);

	while (inside_volume)
	{
		// get sample
		count++;
		//float s = get_sample_data(sampling_pos);

		vec3 s0 = get_sample_data0(sampling_pos);
		vec3 s2 = get_sample_data2(sampling_pos);
		float s3 = get_sample_data3(sampling_pos);
		float s4 = get_sample_data4(sampling_pos);
		float s00 = sqrt(pow(s0.x, 2) + pow(s0.y, 2) + pow(s0.z, 2));
		float s20 = sqrt(pow(s2.x, 2) + pow(s2.y, 2) + pow(s2.z, 2));

		//vec4 color = texture(transfer_texture, vec2(s, s));

		vec4 color0 = texture(transfer_texture, vec2(s00, s00));
		vec4 color2 = texture(transfer_texture2, vec2(s20, s20));
		vec4 color3 = texture(transfer_texture3, vec2(s3, s3));
		vec4 color4 = texture(transfer_texture4, vec2(s4, s4));

		// dummy code
		//average_val.r += color.r;
		//average_val.g += color.g;
		//average_val.b += color.b;
		//average_val.a += color.a;

		average_val0 += color0;
		average_val2 += color2;
		average_val3 += color3;
		average_val4 += color4;


		// increment the ray sampling position
		sampling_pos += ray_increment;

		// update the loop termination condition
		inside_volume = inside_volume_bounds(sampling_pos);

	}

#if ENABLE_VOL0 == 1	


	inten = inten + vec4(average_val0.r/count, average_val0.g/count, average_val0.b/count, average_val0.a/count*3);
#endif
#if ENABLE_VOL2 == 1


	inten = inten + vec4(average_val2.r/count, average_val2.g/count, average_val2.b/count, average_val2.a/count*3);
#endif
#if ENABLE_VOL3 == 1


	inten = inten + vec4(average_val3.r/count, average_val3.g/count, average_val3.b/count, average_val3.a/count*3);
#endif
#if ENABLE_VOL4 == 1


	inten = inten + vec4(average_val4.r/count, average_val4.g/count, average_val4.b/count, average_val4.a/count*3);
#endif


	dst = inten;

	//dst = vec4(average_val.r/count, average_val.g/count, average_val.b/count, average_val.a/count);
#endif

#if TASK == 31 || TASK == 32  // ASSIGNMENT 2 & 3 & 4
	//float s1=0;
	int lighting = 0;
	int shadows = 0;
	vec3 grad = vec3(0.0, 0.0, 0.0);
	vec3 position = vec3(0.0, 0.0, 0.0);

	float ka = 0.2;
	float kd = 0.7;
	float ks = 0.9;
	float exp = 0.9;


	// the traversal loop,
	// termination when the sampling position is outside volume boundarys
	// another termination condition for early ray termination is added
	while (inside_volume)
	{
		// get sample
#if ENABLE_VOL0 == 1	

		vec3 s0 = get_sample_data0(sampling_pos);
		vec3 s0_ = get_sample_data0(sampling_pos + ray_increment);
		float s = sqrt(pow(s0.x, 2) + pow(s0.y, 2) + pow(s0.z, 2));
		float s1 = sqrt(pow(s0_.x, 2) + pow(s0_.y, 2) + pow(s0_.z, 2));
		vec4 color = texture(transfer_texture, vec2(s, s));
#endif
#if ENABLE_VOL2 == 1

		vec3 s2 = get_sample_data2(sampling_pos);
		vec3 s2_ = get_sample_data2(sampling_pos + ray_increment);
		float s= sqrt(pow(s2.x, 2) + pow(s2.y, 2) + pow(s2.z, 2));
		float s1 = sqrt(pow(s2_.x, 2) + pow(s2_.y, 2) + pow(s2_.z, 2));
		vec4 color = texture(transfer_texture2, vec2(s, s));
#endif
#if ENABLE_VOL3 == 1

		float s = get_sample_data3(sampling_pos);
		float s1 = get_sample_data3(sampling_pos + ray_increment);
		vec4 color = texture(transfer_texture3, vec2(s, s));
#endif
#if ENABLE_VOL4 == 1

		float s = get_sample_data4(sampling_pos);
		float s1 = get_sample_data4(sampling_pos + ray_increment);
		vec4 color = texture(transfer_texture4, vec2(s, s));
#endif



		//		float s = get_sample_data(sampling_pos);
		//		float s1 = get_sample_data(sampling_pos + ray_increment);
		float sm = 0.0;





		// dummy code


		if (((s>iso_value) && (s1<iso_value)) || ((s<iso_value) && (s1>iso_value)))

		{
#if TASK == 32 // Binary Search
		{
			vec3 inc = ray_increment / 2;
			sampling_pos += inc;

			if (s<s1)
			{

				for (int i = 0; i<16; i++)
				{
					s = get_sample_data(sampling_pos);
					inc = inc / 2;
					if (s<iso_value)
						sampling_pos += inc;
					else
						sampling_pos -= inc;

				}
			}

			else
			{

				for (int i = 0; i<16; i++)
				{
					s = get_sample_data(sampling_pos);
					inc = inc / 2;
					if (s<iso_value)
						sampling_pos -= inc;
					else
						sampling_pos += inc;


				}
			}
		}

#endif
#if ENABLE_LIGHTNING == 1
			//if (lighting==1)
			{


				//diffuse component
				grad = -get_gradient(sampling_pos);
				vec3 l;
				vec3 light2 = vec3(-light_position.x, -light_position.z, -light_position.y);
				//l=normalize(-light_position-sampling_pos);
				l = normalize(light2 - sampling_pos);
				vec3 n;
				n = normalize(grad.xyz);

				float nandl = max(dot(l, n), 0);

				//specular component
				vec3 r;
				r = reflect(-l, n);


				vec3 v;
				v = normalize(camera_location - sampling_pos);
				float randv = max(dot(r, v), 0);

				//vec4 ambient = vec4(light_ambient_color*ka, 1.0);
				vec4 ambient = vec4(color.rgb*ka, 1.0);
				vec4 diffuse = vec4(kd*nandl*light_diffuse_color, 1.0);
				vec4 specular = vec4(ks*pow(randv, exp)*light_specular_color, 1.0);

#if ENABLE_SHADOWING == 1 // Add Shadows


				/// One step trough the volume
				vec3 ray_inc = normalize(light2 - sampling_pos) * sampling_distance;
				//vec3 ray_inc     = normalize(sampling_pos-light2) * sampling_distance;
				/// Position in Volume
				vec3 sp = sampling_pos + ray_inc;
				/// check if we are inside volume
				bool inside_ray = inside_volume_bounds(sp);
				int oclusion = 0;
				float sl, sl1;
				while (inside_ray)
				{
					sl = get_sample_data(sp);
					sl1 = get_sample_data(sp + ray_inc);
					if (((sl>iso_value) && (sl1<iso_value)) || ((sl<iso_value) && (sl1>iso_value)))
					{
						oclusion = 1;
						break;
					}
					// increment the ray sampling position
					sp += ray_inc;
					// update the loop termination condition
					inside_ray = inside_volume_bounds(sp);

				}
				if (oclusion == 1)
					dst = ambient;
				else
					dst = ambient + diffuse + specular;

#endif
#if ENABLE_SHADOWING != 1 // Add Shadows
				dst = ambient + diffuse + specular;
#endif
			}

#endif
#if ENABLE_LIGHTNING != 1
			//dst = vec4(normalize(grad),1.0);


			//inside_volume = false;

			//else 
			dst = vec4(color.r, color.g, color.b, color.a * 3);
#endif

			//dst = vec4(sampling_pos,1.0);
			break;
		}

		//s1 = s;
		// increment the ray sampling position
		sampling_pos += ray_increment;

		// update the loop termination condition
		inside_volume = inside_volume_bounds(sampling_pos);
	}
#endif 

#if TASK == 41 // ASSIGNMENT 5 & 6 & 7
	// the traversal loop,
	// termination when the sampling position is outside volume boundarys
	// another termination condition for early ray termination is added

	float ka = 0.5;
	float kd = 0.7;
	float ks = 0.9;
	float exp = 0.9;
	vec3 grad = vec3(0.0, 0.0, 0.0);






	float trans = 1.0;
	vec3 inten;
	sampling_pos += ray_increment;

	while (inside_volume && trans > 0.05)
	{

		// get sample
		//float s = get_sample_data(sampling_pos);

		vec3 s0 = get_sample_data0(sampling_pos);
		vec3 s2 = get_sample_data2(sampling_pos);
		float s3 = get_sample_data3(sampling_pos);
		float s4 = get_sample_data4(sampling_pos);
		float s00 = sqrt(pow(s0.x, 2) + pow(s0.y, 2) + pow(s0.z, 2));
		float s20 = sqrt(pow(s2.x, 2) + pow(s2.y, 2) + pow(s2.z, 2));


		//vec4 color = texture(transfer_texture, vec2(s, s));

		vec4 color0 = texture(transfer_texture, vec2(s00, s00));
		vec4 color2 = texture(transfer_texture2, vec2(s20, s20));
		vec4 color3 = texture(transfer_texture3, vec2(s3, s3));
		vec4 color4 = texture(transfer_texture4, vec2(s4, s4));



#if ENABLE_OPACITY_CORRECTION == 1 // Opacity Correction
		color.a = 1 - (pow((1 - color.a), (sampling_distance / sampling_distance_ref) * 100));
#else
		//float s = get_sample_data(sampling_pos);
		//float s_old = get_sample_data(sampling_pos-ray_increment);

#endif
		//front to back algorithm
		//vec4 color = texture(transfer_texture, vec2(s, s));




#if ENABLE_LIGHTNING == 1 // Add Shading
		//diffuse component
		grad = -get_gradient(sampling_pos);
		vec3 l;
		vec3 light2 = vec3(-light_position.x, -light_position.z, -light_position.y);
		//l=normalize(-light_position-sampling_pos);
		l = normalize(light2 - sampling_pos);
		vec3 n;
		n = normalize(grad.xyz);

		float nandl = max(dot(l, n), 0);

		//specular component
		vec3 r;
		r = reflect(-l, n);


		vec3 v;
		v = normalize(camera_location - sampling_pos);
		float randv = max(dot(r, v), 0);

		//			vec4 ambient = vec4(color.rgb*color.a*light_ambient_color, 1.0);
		//			vec4 diffuse = vec4(kd*nandl*(light_diffuse_color, 1.0));
		//			vec4 specular = vec4(ks*pow(randv, exp)*light_specular_color, 1.0);
		//			vec4 total = ambient + diffuse + specular;

		vec4 ambient = vec4(color.rgb*ka, 1.0);
		vec4 diffuse = vec4(kd*nandl*(light_diffuse_color, 1.0));
		vec4 specular = vec4(ks*pow(randv, exp)*light_specular_color, 1.0);
		vec4 total = ambient + diffuse + specular;


		inten = inten + trans*total.rgb*color.a;
		trans = trans*(1 - color.a);
		dst = vec4(inten, 1.0 - trans);
#endif
#if ENABLE_LIGHTNING != 1 




#if ENABLE_VOL0 == 1	
		inten = inten + trans*color0.rgb*color0.a;
		trans = trans*(1 - color0.a);

#endif
#if ENABLE_VOL2 == 1
		inten = inten + trans*color2.rgb*color2.a;
		trans = trans*(1 - color2.a);

#endif
#if ENABLE_VOL3 == 1
		inten = inten + trans*color3.rgb*color3.a;
		trans = trans*(1 - color3.a);

#endif
#if ENABLE_VOL4 == 1
		inten = inten + trans*color4.rgb*color4.a;
		trans = trans*(1 - color4.a);

#endif
		dst = vec4(inten, 1.0 - trans);

#endif

		// increment the ray sampling position
		sampling_pos += ray_increment;
		// update the loop termination condition
		inside_volume = inside_volume_bounds(sampling_pos);
	}
	//dst = vec4(inten, trans);



#endif 
#if TASK == 5

	// the traversal loop,
	// termination when the sampling position is outside volume boundarys
	// another termination condition for early ray termination is added

	float ka = 0.5;
	float kd = 0.7;
	float ks = 0.9;
	float exp = 0.9;

	//vec4 max_val = vec4(0.0, 0.0, 0.0, 0.0);


	vec3 grad = vec3(0.0, 0.0, 0.0);
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);







		float trans = 1.0;
		vec3 inten;
		sampling_pos += ray_increment;

		while (inside_volume && trans > 0.05)
		{

			// get sample
			//float s = get_sample_data(sampling_pos);
#if PAIR == 12
			vec3 s0 = get_sample_data0(sampling_pos);
			vec3 s2 = get_sample_data2(sampling_pos);
			float val1 = sqrt(pow(s0.x, 2) + pow(s0.y, 2) + pow(s0.z, 2));
			float val2 = sqrt(pow(s2.x, 2) + pow(s2.y, 2) + pow(s2.z, 2));
#endif
#if PAIR == 13
			vec3 s0 = get_sample_data0(sampling_pos);
			float val1 = sqrt(pow(s0.x, 2) + pow(s0.y, 2) + pow(s0.z, 2));
			float val2 = get_sample_data3(sampling_pos);
#endif
#if PAIR == 14
			vec3 s0 = get_sample_data0(sampling_pos);
			float val1 = sqrt(pow(s0.x, 2) + pow(s0.y, 2) + pow(s0.z, 2));
			float val2 = get_sample_data4(sampling_pos);
#endif
#if PAIR == 23
			vec3 s2 = get_sample_data2(sampling_pos);
			float val1 = sqrt(pow(s2.x, 2) + pow(s2.y, 2) + pow(s2.z, 2));
			float val2 = get_sample_data3(sampling_pos);
#endif
#if PAIR == 24
			vec3 s2 = get_sample_data2(sampling_pos);
			float val1 = sqrt(pow(s2.x, 2) + pow(s2.y, 2) + pow(s2.z, 2));
			float val2 = get_sample_data4(sampling_pos);
#endif
#if PAIR == 34
			float val1 = get_sample_data3(sampling_pos);
			float val2 = get_sample_data4(sampling_pos);
#endif
			//vec3 s0 = get_sample_data0(sampling_pos);
			//vec3 s2 = get_sample_data2(sampling_pos);
			//float s3 = get_sample_data3(sampling_pos);
			//float s4 = get_sample_data4(sampling_pos);
			//float s00 = sqrt(pow(s0.x, 2) + pow(s0.y, 2) + pow(s0.z, 2));
			//float s20 = sqrt(pow(s2.x, 2) + pow(s2.y, 2) + pow(s2.z, 2));

#if FUNC == 1			
			if((val1>0.52)&&(val2>0.52))	//in range
			{
				if((val1<0.69)&&(val2<0.69))//orange
					color = vec4(255.0,144.0,0.0,1);
				else
					if((val1<0.69)&&(val2>=0.7)&&(val2<=0.8))//light pink
						color = vec4(255,192,203,0.2);
					else
						if((val1<0.69)&&(val2>0.84))//super pink
							color = vec4(255.0,20.0,147.0,0.2);
						else
							if((val1>=0.69)&&(val1<=0.8)&&(val2<0.69))//light green
								color = vec4(152.0,251.0,152.0,0.3);
							else
								if((val1>=0.69)&&(val1<=0.8)&&(val2>=0.69)&&(val2<=0.8))//white
									color = vec4(255.0, 255.0, 255.0, 0.2);
								else
									if((val1>=0.69)&&(val1<=0.8)&&(val2>0.84))//purple
										color = vec4(147.0,112.0,219.0, 0.3);
									else
										if((val1>0.84)&&(val2<0.69))//green
											color = vec4(0.0,128.0,0.0,0.3);
										else
											if((val1>0.84)&&(val2>=0.69)&&(val2<=0.8))//blue
												color = vec4(65,105,225,0.5);
											else
												if((val1>0.84)&&(val2>0.84))//violet
													color = vec4(75.0,0.0,130.0,0.3);
												else
													color = vec4(0.0,0.0,0.0,0.0);
			}
											else
											color = vec4(0.0,0.0,0.0,0.0);
#endif
#if FUNC == 2
			color = texture(multi_transfer1, vec2(val1, val2));
#endif
#if FUNC == 3
			color = texture(multi_transfer2, vec2(val1, val2));
#endif
#if FUNC == 4
			color = texture(multi_transfer3, vec2(val1, val2));
#endif
#if FUNC == 5
			color = texture(multi_transfer4, vec2(val1, val2));
#endif

			//vec4 color0 = texture(transfer_texture, vec2(s00, s00));
			//vec4 color2 = texture(transfer_texture, vec2(s20, s20));
			//vec4 color3 = texture(transfer_texture, vec2(s3, s3));
			//vec4 color4 = texture(transfer_texture, vec2(s4, s4));

			

#if ENABLE_OPACITY_CORRECTION == 1 // Opacity Correction
			color.a = 1 - (pow((1 - color.a), (sampling_distance / sampling_distance_ref) * 100));
//#else
			//float s = get_sample_data(sampling_pos);
			//float s_old = get_sample_data(sampling_pos-ray_increment);

#endif
			//front to back algorithm
			//vec4 color = texture(transfer_texture, vec2(s, s));




#if ENABLE_LIGHTNING == 1 // Add Shading
			//diffuse component
			grad = -get_gradient(sampling_pos);
			vec3 l;
			vec3 light2 = vec3(-light_position.x, -light_position.z, -light_position.y);
			//l=normalize(-light_position-sampling_pos);
			l = normalize(light2 - sampling_pos);
			vec3 n;
			n = normalize(grad.xyz);

			float nandl = max(dot(l, n), 0);

			//specular component
			vec3 r;
			r = reflect(-l, n);


			vec3 v;
			v = normalize(camera_location - sampling_pos);
			float randv = max(dot(r, v), 0);
#if FUNC == 1
			vec4 ambient = vec4(color.rgb*color.a*light_ambient_color, 1.0);

#endif
#if FUNC == 2 || FUNC == 3 || FUNC == 4 || FUNC == 5
			vec4 ambient = vec4(color.rgb*ka, 1.0);

#endif
			vec4 diffuse = vec4(kd*nandl*(light_diffuse_color, 1.0));
			vec4 specular = vec4(ks*pow(randv, exp)*light_specular_color, 1.0);
			vec4 total = ambient + diffuse + specular;
			
			inten = inten + trans*total.rgb*color.a;
			trans = trans*(1 - color.a);
			dst = vec4(inten, 1.0 - trans);

			


#endif
#if ENABLE_LIGHTNING != 1 
			inten = inten + trans*color.rgb*color.a;
			trans = trans*(1 - color.a);
			dst = vec4(inten, 1.0 - trans);

#endif

			// increment the ray sampling position
			sampling_pos += ray_increment;
			// update the loop termination condition
			inside_volume = inside_volume_bounds(sampling_pos);
		}

#endif



	// return the calculated color value
	FragColor = dst;
}
