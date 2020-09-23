#define VERTICES 3

cbuffer CBUFFER : register(b0)
{
	float4x4 world;
	float4x4 view;
	float4x4 project;
};

struct GS_IN
{
	float4 VPos : SV_POSITION;
	float2 Tex : TEXCOORD;
};

struct GS_OUT
{
	float4 VPos : SV_POSITION; //Vertex Position
	float4 pointPos : POSITION; //Point on surface position
	float4 Normal : NORMAL; //Normal
	float2 Tex : TEXCOORD; //UV coordinates
};


[maxvertexcount(6)]
void GS_main(triangle GS_IN input[3], inout TriangleStream<GS_OUT> PS_Stream)
{
	GS_OUT output = (GS_OUT)0;
	float4x4 transformation = mul(world, mul(view, project)); 
	float4 normal = normalize(float4( cross(input[1].VPos - input[0].VPos, input[2].VPos - input[0].VPos), 0.0f ));
	
	for (int i = 0; i < VERTICES; i++)
	{
		output.VPos = mul(input[i].VPos, transformation); //Project vertices.
		output.pointPos = mul(input[i].VPos, world); //Multiply with world matrix, not whole transformation
		output.Normal = mul(normal, transformation); //Project.
		output.Tex = input[i].Tex; //Copy texture coordinates.
		PS_Stream.Append(output);
	}
	PS_Stream.RestartStrip();
	//Create a duplicate triangle
	for (int i = 0; i<VERTICES; i++)
	{
		output.VPos = mul(input[i].VPos + normal, transformation); //Move the vertices of the duplicated triangle by the length of the triangle's normal.
		output.pointPos = mul(input[i].VPos, world);
		output.Normal = mul(normal, world);
		output.Tex = input[i].Tex;
		PS_Stream.Append(output);
	}
	//Main executes once per triangle, meaning that we duplicate both triangles we insert
}