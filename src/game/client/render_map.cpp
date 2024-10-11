/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <math.h>
#include <base/math.h>
#include <base/tl/base.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <generated/client_data.h>

#include "render.h"

void ValidateFCurve(const vec2& p0, vec2& p1, vec2& p2, const vec2& p3)
{
	// validate the bezier curve
	p1.x = clamp(p1.x, p0.x, p3.x);
	p2.x = clamp(p2.x, p0.x, p3.x);
}

double CubicRoot(double x)
{
	if(x == 0.0)
		return 0.0;
	else if(x < 0.0)
		return -exp(log(-x)/3.0);
	else
		return exp(log(x)/3.0);
}

float SolveBezier(float x, float p0, float p1, float p2, float p3)
{
	// check for valid f-curve
	// we only take care of monotonic bezier curves, so there has to be exactly 1 real solution
	tl_assert(p0 <= x && x <= p3);
	tl_assert((p0 <= p1 && p1 <= p3) && (p0 <= p2 && p2 <= p3));

	double a, b, c, t;
	double x3 = -p0 + 3*p1 - 3*p2 + p3;
	double x2 = 3*p0 - 6*p1 + 3*p2;
	double x1 = -3*p0 + 3*p1;
	double x0 = p0 - x;

	if(x3 == 0.0 && x2 == 0.0)
	{
		// linear
		// a*t + b = 0
		a = x1;
		b = x0;

		if(a == 0.0)
			return 0.0f;
		else
			return -b/a;
	}
	else if(x3 == 0.0)
	{
		// quadratic
		// t*t + b*t +c = 0
		b = x1/x2;
		c = x0/x2;

		if(c == 0.0)
			return 0.0f;

		double D = b*b - 4*c;

		t = (-b + sqrt(D))/2;

		if(0.0 <= t && t <= 1.0001f)
			return t;
		else
			return (-b - sqrt(D))/2;
	}
	else
	{
		// cubic
		// t*t*t + a*t*t + b*t*t + c = 0
		a = x2 / x3;
		b = x1 / x3;
		c = x0 / x3;

		// substitute t = y - a/3
		double sub = a/3.0;

		// depressed form x^3 + px + q = 0
		// cardano's method
		double p = b/3 - a*a/9;
		double q = (2*a*a*a/27 - a*b/3 + c)/2;

		double D = q*q + p*p*p;

		if(D > 0.0)
		{
			// only one 'real' solution
			double s = sqrt(D);
			return CubicRoot(s-q) - CubicRoot(s+q) - sub;
		}
		else if(D == 0.0)
		{
			// one single, one double solution or triple solution
			double s = CubicRoot(-q);
			t = 2*s - sub;

			if(0.0 <= t && t <= 1.0001f)
				return t;
			else
				return (-s - sub);

		}
		else
		{
			// Casus irreductibilis ... ,_,
			double phi = acos(-q / sqrt(-(p*p*p))) / 3;
			double s = 2*sqrt(-p);

			t = s*cos(phi) - sub;

			if(0.0 <= t && t <= 1.0001f)
				return t;

			t = -s*cos(phi+pi/3) - sub;

			if(0.0 <= t && t <= 1.0001f)
				return t;
			else
				return -s*cos(phi-pi/3) - sub;
		}
	}
}

void CRenderTools::RenderEvalEnvelope(CEnvPoint *pPoints, int NumPoints, int Channels, float Time, float *pResult)
{
	if(NumPoints == 0)
	{
		pResult[0] = 0;
		pResult[1] = 0;
		pResult[2] = 0;
		pResult[3] = 0;
		return;
	}

	if(NumPoints == 1)
	{
		pResult[0] = fx2f(pPoints[0].m_aValues[0]);
		pResult[1] = fx2f(pPoints[0].m_aValues[1]);
		pResult[2] = fx2f(pPoints[0].m_aValues[2]);
		pResult[3] = fx2f(pPoints[0].m_aValues[3]);
		return;
	}

	Time = fmod(Time, pPoints[NumPoints-1].m_Time/1000.0f)*1000.0f;

	for(int i = 0; i < NumPoints-1; i++)
	{
		if(Time >= pPoints[i].m_Time && Time <= pPoints[i+1].m_Time)
		{
			float Delta = pPoints[i+1].m_Time-pPoints[i].m_Time;
			float a = (Time-pPoints[i].m_Time)/Delta;

			switch(pPoints[i].m_Curvetype)
			{
			case CURVETYPE_STEP:
				a = 0;
				break;
			case CURVETYPE_SMOOTH:
				a = -2*a*a*a + 3*a*a; // second hermite basis
				break;
			case CURVETYPE_SLOW:
				a = a*a*a;
				break;
			case CURVETYPE_FAST:
				a = 1-a;
				a = 1-a*a*a;
				break;
			case CURVETYPE_BEZIER:
				for(int c = 0; c < Channels; c++)
				{
					// monotonic 2d cubic bezier curve
					vec2 p0, p1, p2, p3;
					vec2 inTang, outTang;

					p0 = vec2(pPoints[i].m_Time/1000.0f, fx2f(pPoints[i].m_aValues[c]));
					p3 = vec2(pPoints[i+1].m_Time/1000.0f, fx2f(pPoints[i+1].m_aValues[c]));

					outTang = vec2(pPoints[i].m_aOutTangentdx[c]/1000.0f, fx2f(pPoints[i].m_aOutTangentdy[c]));
					inTang = -vec2(pPoints[i+1].m_aInTangentdx[c]/1000.0f, fx2f(pPoints[i+1].m_aInTangentdy[c]));
					p1 = p0 + outTang;
					p2 = p3 - inTang;

					// validate bezier curve
					ValidateFCurve(p0, p1, p2, p3);

					// solve x(a) = time for a
					a = clamp(SolveBezier(Time/1000.0f, p0.x, p1.x, p2.x, p3.x), 0.0f, 1.0f);

					// value = y(t)
					pResult[c] =  bezier(p0.y, p1.y, p2.y, p3.y, a);
				}
				return;
			case CURVETYPE_LINEAR:
				break;
			}

			for(int c = 0; c < Channels; c++)
			{
				float v0 = fx2f(pPoints[i].m_aValues[c]);
				float v1 = fx2f(pPoints[i+1].m_aValues[c]);
				pResult[c] = mix(v0, v1, a);
			}

			return;
		}
	}

	pResult[0] = fx2f(pPoints[NumPoints-1].m_aValues[0]);
	pResult[1] = fx2f(pPoints[NumPoints-1].m_aValues[1]);
	pResult[2] = fx2f(pPoints[NumPoints-1].m_aValues[2]);
	pResult[3] = fx2f(pPoints[NumPoints-1].m_aValues[3]);
	return;
}

static void Rotate(CPoint *pCenter, CPoint *pPoint, float Rotation)
{
	int x = pPoint->x - pCenter->x;
	int y = pPoint->y - pCenter->y;
	pPoint->x = (int)(x * cosf(Rotation) - y * sinf(Rotation) + pCenter->x);
	pPoint->y = (int)(x * sinf(Rotation) + y * cosf(Rotation) + pCenter->y);
}

void CRenderTools::RenderQuads(CQuad *pQuads, int NumQuads, int RenderFlags, ENVELOPE_EVAL pfnEval, void *pUser)
{
	if(!m_pConfig->m_ClShowQuads || m_pConfig->m_ClOverlayEntities == 100)
		return;
	Graphics()->QuadsBegin();
	float Conv = 1/255.0f;
	for(int i = 0; i < NumQuads; i++)
	{
		CQuad *q = &pQuads[i];

		float r=1, g=1, b=1, a=1;

		if(q->m_ColorEnv >= 0)
		{
			float aChannels[4];
			pfnEval(q->m_ColorEnvOffset/1000.0f, q->m_ColorEnv, aChannels, pUser);
			r = aChannels[0];
			g = aChannels[1];
			b = aChannels[2];
			a = aChannels[3];
		}

		/*bool Opaque = false;
		 TODO: Analyze quadtexture
		if(a < 0.01f || (q->m_aColors[0].a < 0.01f && q->m_aColors[1].a < 0.01f && q->m_aColors[2].a < 0.01f && q->m_aColors[3].a < 0.01f))
			Opaque = true;

		if(Opaque && !(RenderFlags&LAYERRENDERFLAG_OPAQUE))
			continue;
		if(!Opaque && !(RenderFlags&LAYERRENDERFLAG_TRANSPARENT))
			continue;
		*/
		vec2 aTexCoords[4];
		for(int k = 0; k < 4; k++)
		{
			aTexCoords[k].x = fx2f(q->m_aTexcoords[k].x);
			aTexCoords[k].y = fx2f(q->m_aTexcoords[k].y);
		}

		// Check if we want to repeat the texture
		// Otherwise clamp to the edge to prevent texture bleeding
		bool RepeatU = false, RepeatV = false;
		for(int k = 0; k < 4; k++)
		{
			if(aTexCoords[k].x < 0.0f || aTexCoords[k].x > 1.0f)
				RepeatU = true;
			if(aTexCoords[k].y < 0.0f || aTexCoords[k].y > 1.0f)
				RepeatV = true;
		}
		Graphics()->WrapMode(
			RepeatU ? IGraphics::WRAP_REPEAT : IGraphics::WRAP_CLAMP,
			RepeatV ? IGraphics::WRAP_REPEAT : IGraphics::WRAP_CLAMP);

		Graphics()->QuadsSetSubsetFree(
			aTexCoords[0].x, aTexCoords[0].y,
			aTexCoords[1].x, aTexCoords[1].y,
			aTexCoords[2].x, aTexCoords[2].y,
			aTexCoords[3].x, aTexCoords[3].y);

		float OffsetX = 0;
		float OffsetY = 0;
		float Rot = 0;

		// TODO: fix this
		if(q->m_PosEnv >= 0)
		{
			float aChannels[4];
			pfnEval(q->m_PosEnvOffset/1000.0f, q->m_PosEnv, aChannels, pUser);
			OffsetX = aChannels[0];
			OffsetY = aChannels[1];
			Rot = aChannels[2]/360.0f*pi*2;
		}

		int EntitiesA = 100-m_pConfig->m_ClOverlayEntities;
		IGraphics::CColorVertex Array[4] = {
			IGraphics::CColorVertex(0, q->m_aColors[0].r*Conv*r*q->m_aColors[0].a*Conv*a, q->m_aColors[0].g*Conv*g*q->m_aColors[0].a*Conv*a, q->m_aColors[0].b*Conv*b*q->m_aColors[0].a*Conv*a, q->m_aColors[0].a*Conv*a*EntitiesA/100.0f),
			IGraphics::CColorVertex(1, q->m_aColors[1].r*Conv*r*q->m_aColors[1].a*Conv*a, q->m_aColors[1].g*Conv*g*q->m_aColors[1].a*Conv*a, q->m_aColors[1].b*Conv*b*q->m_aColors[1].a*Conv*a, q->m_aColors[1].a*Conv*a*EntitiesA/100.0f),
			IGraphics::CColorVertex(2, q->m_aColors[2].r*Conv*r*q->m_aColors[2].a*Conv*a, q->m_aColors[2].g*Conv*g*q->m_aColors[2].a*Conv*a, q->m_aColors[2].b*Conv*b*q->m_aColors[2].a*Conv*a, q->m_aColors[2].a*Conv*a*EntitiesA/100.0f),
			IGraphics::CColorVertex(3, q->m_aColors[3].r*Conv*r*q->m_aColors[3].a*Conv*a, q->m_aColors[3].g*Conv*g*q->m_aColors[3].a*Conv*a, q->m_aColors[3].b*Conv*b*q->m_aColors[3].a*Conv*a, q->m_aColors[3].a*Conv*a*EntitiesA/100.0f)};
			/*
			IGraphics::CColorVertex(0, q->m_aColors[0].r*Conv*r*q->m_aColors[0].a*Conv*a, q->m_aColors[0].g*Conv*g*q->m_aColors[0].a*Conv*a, q->m_aColors[0].b*Conv*b*q->m_aColors[0].a*Conv*a, q->m_aColors[0].a*Conv*a),
			IGraphics::CColorVertex(1, q->m_aColors[1].r*Conv*r*q->m_aColors[1].a*Conv*a, q->m_aColors[1].g*Conv*g*q->m_aColors[1].a*Conv*a, q->m_aColors[1].b*Conv*b*q->m_aColors[1].a*Conv*a, q->m_aColors[1].a*Conv*a),
			IGraphics::CColorVertex(2, q->m_aColors[2].r*Conv*r*q->m_aColors[2].a*Conv*a, q->m_aColors[2].g*Conv*g*q->m_aColors[2].a*Conv*a, q->m_aColors[2].b*Conv*b*q->m_aColors[2].a*Conv*a, q->m_aColors[2].a*Conv*a),
			IGraphics::CColorVertex(3, q->m_aColors[3].r*Conv*r*q->m_aColors[3].a*Conv*a, q->m_aColors[3].g*Conv*g*q->m_aColors[3].a*Conv*a, q->m_aColors[3].b*Conv*b*q->m_aColors[3].a*Conv*a, q->m_aColors[3].a*Conv*a)};
			*/
			/*
			IGraphics::CColorVertex(0, q->m_aColors[0].r*Conv*r, q->m_aColors[0].g*Conv*g, q->m_aColors[0].b*Conv*b, q->m_aColors[0].a*Conv*a*(100-m_pConfig->m_ClOverlayEntities)/100.0f),
			IGraphics::CColorVertex(1, q->m_aColors[1].r*Conv*r, q->m_aColors[1].g*Conv*g, q->m_aColors[1].b*Conv*b, q->m_aColors[1].a*Conv*a*(100-m_pConfig->m_ClOverlayEntities)/100.0f),
			IGraphics::CColorVertex(2, q->m_aColors[2].r*Conv*r, q->m_aColors[2].g*Conv*g, q->m_aColors[2].b*Conv*b, q->m_aColors[2].a*Conv*a*(100-m_pConfig->m_ClOverlayEntities)/100.0f),
			IGraphics::CColorVertex(3, q->m_aColors[3].r*Conv*r, q->m_aColors[3].g*Conv*g, q->m_aColors[3].b*Conv*b, q->m_aColors[3].a*Conv*a*(100-m_pConfig->m_ClOverlayEntities)/100.0f)};
			*/
		Graphics()->SetColorVertex(Array, 4);

		CPoint *pPoints = q->m_aPoints;

		if(Rot != 0)
		{
			static CPoint aRotated[4];
			aRotated[0] = q->m_aPoints[0];
			aRotated[1] = q->m_aPoints[1];
			aRotated[2] = q->m_aPoints[2];
			aRotated[3] = q->m_aPoints[3];
			pPoints = aRotated;

			Rotate(&q->m_aPoints[4], &aRotated[0], Rot);
			Rotate(&q->m_aPoints[4], &aRotated[1], Rot);
			Rotate(&q->m_aPoints[4], &aRotated[2], Rot);
			Rotate(&q->m_aPoints[4], &aRotated[3], Rot);
		}

		IGraphics::CFreeformItem Freeform(
			fx2f(pPoints[0].x)+OffsetX, fx2f(pPoints[0].y)+OffsetY,
			fx2f(pPoints[1].x)+OffsetX, fx2f(pPoints[1].y)+OffsetY,
			fx2f(pPoints[2].x)+OffsetX, fx2f(pPoints[2].y)+OffsetY,
			fx2f(pPoints[3].x)+OffsetX, fx2f(pPoints[3].y)+OffsetY);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	}
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();
}

void CRenderTools::RenderTilemap(CTile *pTiles, int w, int h, float Scale, vec4 Color, int RenderFlags,
									ENVELOPE_EVAL pfnEval, void *pUser, int ColorEnv, int ColorEnvOffset)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	float r=1, g=1, b=1, a=1;
	if(ColorEnv >= 0)
	{
		float aChannels[4];
		pfnEval(ColorEnvOffset/1000.0f, ColorEnv, aChannels, pUser);
		r = aChannels[0];
		g = aChannels[1];
		b = aChannels[2];
		a = aChannels[3];
	}

	Graphics()->QuadsBegin();
	const float Alpha = Color.a*a;
	Graphics()->SetColor(Color.r*r*Alpha, Color.g*g*Alpha, Color.b*b*Alpha, Alpha);

	int StartY = (int)(ScreenY0/Scale)-1;
	int StartX = (int)(ScreenX0/Scale)-1;
	int EndY = (int)(ScreenY1/Scale)+1;
	int EndX = (int)(ScreenX1/Scale)+1;

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(RenderFlags&TILERENDERFLAG_EXTEND)
			{
				if(mx<0)
					mx = 0;
				if(mx>=w)
					mx = w-1;
				if(my<0)
					my = 0;
				if(my>=h)
					my = h-1;
			}
			else
			{
				if(mx<0)
					continue; // mx = 0;
				if(mx>=w)
					continue; // mx = w-1;
				if(my<0)
					continue; // my = 0;
				if(my>=h)
					continue; // my = h-1;
			}

			int c = mx + my*w;

			unsigned char Index = pTiles[c].m_Index;
			if(Index)
			{
				unsigned char Flags = pTiles[c].m_Flags;

				bool Render = false;
				if(Flags&TILEFLAG_OPAQUE && Color.a*a > 254.0f/255.0f)
				{
					if(RenderFlags&LAYERRENDERFLAG_OPAQUE)
						Render = true;
				}
				else
				{
					if(RenderFlags&LAYERRENDERFLAG_TRANSPARENT)
						Render = true;
				}

				if(Render)
				{
					float x0 = 0;
					float y0 = 0;
					float x1 = 1;
					float y1 = 0;
					float x2 = 1;
					float y2 = 1;
					float x3 = 0;
					float y3 = 1;

					if(Flags&TILEFLAG_VFLIP)
					{
						x0 = x2;
						x1 = x3;
						x2 = x3;
						x3 = x0;
					}

					if(Flags&TILEFLAG_HFLIP)
					{
						y0 = y3;
						y2 = y1;
						y3 = y1;
						y1 = y0;
					}

					if(Flags&TILEFLAG_ROTATE)
					{
						float Tmp = x0;
						x0 = x3;
						x3 = x2;
						x2 = x1;
						x1 = Tmp;
						Tmp = y0;
						y0 = y3;
						y3 = y2;
						y2 = y1;
						y1 = Tmp;
					}

					Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3, Index);
					IGraphics::CQuadItem QuadItem(x*Scale, y*Scale, Scale, Scale);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
			x += pTiles[c].m_Skip;
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

// DDRace

void CRenderTools::RenderTeleOverlay(CTeleTile *pTele, int w, int h, float Scale, float Alpha)
{
	if(!m_pConfig->m_ClTextEntities)
	  return;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int StartY = (int)(ScreenY0/Scale)-1;
	int StartX = (int)(ScreenX0/Scale)-1;
	int EndY = (int)(ScreenY1/Scale)+1;
	int EndX = (int)(ScreenX1/Scale)+1;

	if(EndX - StartX > Graphics()->ScreenWidth() / m_pConfig->m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / m_pConfig->m_GfxTextOverlay)
		return; // its useless to render text at this distance

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;


			if(mx<0)
				continue; // mx = 0;
			if(mx>=w)
				continue; // mx = w-1;
			if(my<0)
				continue; // my = 0;
			if(my>=h)
				continue; // my = h-1;

			int c = mx + my*w;

			// TODO: zillywoods
			// unsigned char Index = pTele[c].m_Number;
			// if(Index && pTele[c].m_Type != TILE_TELECHECKIN && pTele[c].m_Type != TILE_TELECHECKINEVIL)
			// {
			// 	char aBuf[16];
			// 	str_format(aBuf, sizeof(aBuf), "%d", Index);
			// 	UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
			// 	UI()->TextRender()->Text(0, mx*Scale - 3.f, my*Scale, Scale - 5.f, aBuf, -1);
			// 	UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			// }
		}

	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderSpeedupOverlay(CSpeedupTile *pSpeedup, int w, int h, float Scale, float Alpha)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int StartY = (int)(ScreenY0/Scale)-1;
	int StartX = (int)(ScreenX0/Scale)-1;
	int EndY = (int)(ScreenY1/Scale)+1;
	int EndX = (int)(ScreenX1/Scale)+1;

	if(EndX - StartX > Graphics()->ScreenWidth() / m_pConfig->m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / m_pConfig->m_GfxTextOverlay)
		return; // its useless to render text at this distance

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(mx<0)
				continue; // mx = 0;
			if(mx>=w)
				continue; // mx = w-1;
			if(my<0)
				continue; // my = 0;
			if(my>=h)
				continue; // my = h-1;

			int c = mx + my*w;

			int Force = (int)pSpeedup[c].m_Force;
			int MaxSpeed = (int)pSpeedup[c].m_MaxSpeed;
			if(Force)
			{
				// draw arrow
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_SPEEDUP_ARROW].m_Id);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(255.0f, 255.0f, 255.0f, Alpha);

				SelectSprite(SPRITE_SPEEDUP_ARROW);
				Graphics()->QuadsSetRotation(pSpeedup[c].m_Angle*(3.14159265f/180.0f));
				DrawSprite(mx*Scale+16, my*Scale+16, 35.0f);

				Graphics()->QuadsEnd();

				// TODO: zillywoods
				// if(m_pConfig->m_ClTextEntities)
				// {
				// 	// draw force
				// 	char aBuf[16];
				// 	str_format(aBuf, sizeof(aBuf), "%d", Force);
				// 	UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
				// 	UI()->TextRender()->Text(0, mx*Scale, (my*Scale) + 16.f + ((16.f - (Scale - 20.f)) / 2.f), Scale - 20.f, aBuf, -1);
				// 	UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
				// 	if(MaxSpeed)
				// 	{
				// 		str_format(aBuf, sizeof(aBuf), "%d", MaxSpeed);
				// 		UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
				// 		UI()->TextRender()->Text(0, mx*Scale, (my*Scale) + ((16.f - (Scale - 20.f)) / 2.f), Scale - 20.f, aBuf, -1);
				// 		UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
				// 	}
				// }
			}
		}
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderSwitchOverlay(CSwitchTile *pSwitch, int w, int h, float Scale, float Alpha)
{
	if(!m_pConfig->m_ClTextEntities)
	  return;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int StartY = (int)(ScreenY0/Scale)-1;
	int StartX = (int)(ScreenX0/Scale)-1;
	int EndY = (int)(ScreenY1/Scale)+1;
	int EndX = (int)(ScreenX1/Scale)+1;

	if(EndX - StartX > Graphics()->ScreenWidth() / m_pConfig->m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / m_pConfig->m_GfxTextOverlay)
		return; // its useless to render text at this distance

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;


			if(mx<0)
				continue; // mx = 0;
			if(mx>=w)
				continue; // mx = w-1;
			if(my<0)
				continue; // my = 0;
			if(my>=h)
				continue; // my = h-1;

			int c = mx + my*w;

			// TODO: zillywoods
			// unsigned char Index = pSwitch[c].m_Number;
			// if(Index)
			// {
			// 	char aBuf[16];
			// 	str_format(aBuf, sizeof(aBuf), "%d", Index);
			// 	UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
			// 	UI()->TextRender()->Text(0, mx*Scale, my*Scale + 16.f + ((16.f - (Scale - 20.f)) / 2.f), Scale - 20.f, aBuf, -1);
			// 	UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			// }

			// unsigned char Delay = pSwitch[c].m_Delay;
			// if(Delay)
			// {
			// 	char aBuf[16];
			// 	str_format(aBuf, sizeof(aBuf), "%d", Delay);
			// 	UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
			// 	UI()->TextRender()->Text(0, mx*Scale, my*Scale + ((16.f - (Scale - 20.f)) / 2.f), Scale - 20.f, aBuf, -1);
			// 	UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			// }
		}

	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderTuneOverlay(CTuneTile *pTune, int w, int h, float Scale, float Alpha)
{
	if(!m_pConfig->m_ClTextEntities)
	  return;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int StartY = (int)(ScreenY0/Scale)-1;
	int StartX = (int)(ScreenX0/Scale)-1;
	int EndY = (int)(ScreenY1/Scale)+1;
	int EndX = (int)(ScreenX1/Scale)+1;

	if(EndX - StartX > Graphics()->ScreenWidth() / m_pConfig->m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / m_pConfig->m_GfxTextOverlay)
		return; // its useless to render text at this distance

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;


			if(mx<0)
				continue; // mx = 0;
			if(mx>=w)
				continue; // mx = w-1;
			if(my<0)
				continue; // my = 0;
			if(my>=h)
				continue; // my = h-1;

			int c = mx + my*w;

			unsigned char Index = pTune[c].m_Number;
			if(Index)
			{
				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d", Index);
				UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
				static CTextCursor s_Cursor;
				s_Cursor.Reset();
				s_Cursor.MoveTo(mx*Scale+11.f, my*Scale+6.f);
				s_Cursor.m_FontSize = Scale/1.5f-5.f;
				s_Cursor.m_MaxLines = -1;
				s_Cursor.m_Align = TEXTALIGN_RIGHT;
				UI()->TextRender()->TextOutlined(&s_Cursor, aBuf, -1.0f);
				UI()->TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}

	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderTelemap(CTeleTile *pTele, int w, int h, float Scale, vec4 Color, int RenderFlags)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);

	int StartY = (int)(ScreenY0/Scale)-1;
	int StartX = (int)(ScreenX0/Scale)-1;
	int EndY = (int)(ScreenY1/Scale)+1;
	int EndX = (int)(ScreenX1/Scale)+1;

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(RenderFlags&TILERENDERFLAG_EXTEND)
			{
				if(mx<0)
					mx = 0;
				if(mx>=w)
					mx = w-1;
				if(my<0)
					my = 0;
				if(my>=h)
					my = h-1;
			}
			else
			{
				if(mx<0)
					continue; // mx = 0;
				if(mx>=w)
					continue; // mx = w-1;
				if(my<0)
					continue; // my = 0;
				if(my>=h)
					continue; // my = h-1;
			}

			int c = mx + my*w;

			unsigned char Index = pTele[c].m_Type;
			if(Index)
			{
				bool Render = false;
				if(RenderFlags&LAYERRENDERFLAG_TRANSPARENT)
					Render = true;

				if(Render)
				{
					float x0 = 0;
					float y0 = 0;
					float x1 = 1;
					float y1 = 0;
					float x2 = 1;
					float y2 = 1;
					float x3 = 0;
					float y3 = 1;

					Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3, Index);
					IGraphics::CQuadItem QuadItem(x*Scale, y*Scale, Scale, Scale);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderSpeedupmap(CSpeedupTile *pSpeedupTile, int w, int h, float Scale, vec4 Color, int RenderFlags)
{
	//Graphics()->TextureSet(img_get(tmap->image));
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	//Graphics()->MapScreen(screen_x0-50, screen_y0-50, screen_x1+50, screen_y1+50);

	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);

	int StartY = (int)(ScreenY0/Scale)-1;
	int StartX = (int)(ScreenX0/Scale)-1;
	int EndY = (int)(ScreenY1/Scale)+1;
	int EndX = (int)(ScreenX1/Scale)+1;

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(RenderFlags&TILERENDERFLAG_EXTEND)
			{
				if(mx<0)
					mx = 0;
				if(mx>=w)
					mx = w-1;
				if(my<0)
					my = 0;
				if(my>=h)
					my = h-1;
			}
			else
			{
				if(mx<0)
					continue; // mx = 0;
				if(mx>=w)
					continue; // mx = w-1;
				if(my<0)
					continue; // my = 0;
				if(my>=h)
					continue; // my = h-1;
			}

			int c = mx + my*w;

			unsigned char Index = pSpeedupTile[c].m_Type;
			if(Index)
			{
				bool Render = false;
				if(RenderFlags&LAYERRENDERFLAG_TRANSPARENT)
					Render = true;

				if(Render)
				{
					float x0 = 0;
					float y0 = 0;
					float x1 = 1;
					float y1 = 0;
					float x2 = 1;
					float y2 = 1;
					float x3 = 0;
					float y3 = 1;

					Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3, Index);
					IGraphics::CQuadItem QuadItem(x*Scale, y*Scale, Scale, Scale);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderSwitchmap(CSwitchTile *pSwitchTile, int w, int h, float Scale, vec4 Color, int RenderFlags)
{
	//Graphics()->TextureSet(img_get(tmap->image));
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	//Graphics()->MapScreen(screen_x0-50, screen_y0-50, screen_x1+50, screen_y1+50);

	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);

	int StartY = (int)(ScreenY0/Scale)-1;
	int StartX = (int)(ScreenX0/Scale)-1;
	int EndY = (int)(ScreenY1/Scale)+1;
	int EndX = (int)(ScreenX1/Scale)+1;

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(RenderFlags&TILERENDERFLAG_EXTEND)
			{
				if(mx<0)
					mx = 0;
				if(mx>=w)
					mx = w-1;
				if(my<0)
					my = 0;
				if(my>=h)
					my = h-1;
			}
			else
			{
				if(mx<0)
					continue; // mx = 0;
				if(mx>=w)
					continue; // mx = w-1;
				if(my<0)
					continue; // my = 0;
				if(my>=h)
					continue; // my = h-1;
			}

			int c = mx + my*w;

			unsigned char Index = pSwitchTile[c].m_Type;
			if(Index)
			{
				if(Index == TILE_SWITCHTIMEDOPEN)
					Index = 8;

				unsigned char Flags = pSwitchTile[c].m_Flags;

				bool Render = false;
				if(Flags&TILEFLAG_OPAQUE)
				{
					if(RenderFlags&LAYERRENDERFLAG_OPAQUE)
						Render = true;
				}
				else
				{
					if(RenderFlags&LAYERRENDERFLAG_TRANSPARENT)
						Render = true;
				}

				if(Render)
				{
					float x0 = 0;
					float y0 = 0;
					float x1 = 1;
					float y1 = 0;
					float x2 = 1;
					float y2 = 1;
					float x3 = 0;
					float y3 = 1;

					if(Flags&TILEFLAG_VFLIP)
					{
						x0 = x2;
						x1 = x3;
						x2 = x3;
						x3 = x0;
					}

					if(Flags&TILEFLAG_HFLIP)
					{
						y0 = y3;
						y2 = y1;
						y3 = y1;
						y1 = y0;
					}

					if(Flags&TILEFLAG_ROTATE)
					{
						float Tmp = x0;
						x0 = x3;
						x3 = x2;
						x2 = x1;
						x1 = Tmp;
						Tmp = y0;
						y0 = y3;
						y3 = y2;
						y2 = y1;
						y1 = Tmp;
					}

					Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3, Index);
					IGraphics::CQuadItem QuadItem(x*Scale, y*Scale, Scale, Scale);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderTools::RenderTunemap(CTuneTile *pTune, int w, int h, float Scale, vec4 Color, int RenderFlags)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);

	int StartY = (int)(ScreenY0/Scale)-1;
	int StartX = (int)(ScreenX0/Scale)-1;
	int EndY = (int)(ScreenY1/Scale)+1;
	int EndX = (int)(ScreenX1/Scale)+1;

	for(int y = StartY; y < EndY; y++)
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(RenderFlags&TILERENDERFLAG_EXTEND)
			{
				if(mx<0)
					mx = 0;
				if(mx>=w)
					mx = w-1;
				if(my<0)
					my = 0;
				if(my>=h)
					my = h-1;
			}
			else
			{
				if(mx<0)
					continue; // mx = 0;
				if(mx>=w)
					continue; // mx = w-1;
				if(my<0)
					continue; // my = 0;
				if(my>=h)
					continue; // my = h-1;
			}

			int c = mx + my*w;

			unsigned char Index = pTune[c].m_Type;
			if(Index)
			{
				bool Render = false;
				if(RenderFlags&LAYERRENDERFLAG_TRANSPARENT)
					Render = true;

				if(Render)
				{
					float x0 = 0;
					float y0 = 0;
					float x1 = 1;
					float y1 = 0;
					float x2 = 1;
					float y2 = 1;
					float x3 = 0;
					float y3 = 1;

					Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3, Index);
					IGraphics::CQuadItem QuadItem(x*Scale, y*Scale, Scale, Scale);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}
			}
		}

	Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

