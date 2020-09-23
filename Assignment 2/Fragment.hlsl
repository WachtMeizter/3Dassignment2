//Code lifted from lectures
Texture2D txDiffuse : register(t0);
SamplerState sampAni;

struct PS_IN
{
	float4 Pos : SV_POSITION;
	float4 Normal : NORMAL;
	float4 pointPos: POSITION;
	float2 Tex : TEXCOORD;
};

float4 PS_main(PS_IN input) : SV_Target
{
	float4 light = {0.0f, 0.0f, -2.0f, 1.0f}; //Light, position is camera position
	float4 dir = normalize(light - input.pointPos); //Direction vector of the light, normalized
	float diffuse = max(dot(dir, input.Normal), 0); //Calculate diffuse lighting
	float4 finalcolor = float4(txDiffuse.Sample(sampAni, input.Tex).xyz, 1.0f); //Sample from the texture created in initTextures()
	finalcolor = saturate(finalcolor * diffuse); //Multiply color with diffuse factor, then clamp between [0, 1] in case finalColor is massive for some reason.
	return finalcolor;
};