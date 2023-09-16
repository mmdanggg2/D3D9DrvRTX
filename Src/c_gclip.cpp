
#include "c_gclip.h"


CGClip::CGClip() {
	unsigned int u;

	m_cpEnableBits = 0x0;

	for (u = 0; u < NUM_CP; u++) {
		m_cp[u].n.x = 0.0f;
		m_cp[u].n.y = 0.0f;
		m_cp[u].n.z = 0.0f;
		m_cp[u].d = 0.0f;
	}
}

CGClip::~CGClip() {
}


void CGClip::SetCpEnable(unsigned int cpNum, bool cpEnable) {
	if (cpNum >= NUM_CP) {
		return;
	}

	if (cpEnable) {
		m_cpEnableBits |= 1U << cpNum;
	}
	else {
		m_cpEnableBits &= ~(1U << cpNum);
	}

	return;
}

void CGClip::SetCp(unsigned int cpNum, const float *pPlane) {
	if (cpNum >= NUM_CP) {
		return;
	}

	m_cp[cpNum].n.x = pPlane[0];
	m_cp[cpNum].n.y = pPlane[1];
	m_cp[cpNum].n.z = pPlane[2];
	m_cp[cpNum].d = pPlane[3];

	return;
}


float CGClip::Dot3(const vec3_t &v1, const vec3_t &v2) {
	return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
}


void CGClip::ClipLine(cl_line_t &clLine) {
	unsigned int cpBitsRemain;
	unsigned int cpBit;
	unsigned int cpNum;

	//Clip to all enabled planes
	cpBitsRemain = m_cpEnableBits;
	for (cpNum = 0, cpBit = 0x1; cpBitsRemain != 0; cpNum++, cpBit <<= 1) {
		//Clip to plane if enabled
		if (cpBit & cpBitsRemain) {
			const plane_t &clPlane = m_cp[cpNum];
			float lineDot[2];
			bool lineIn[2];
			unsigned int idxIn, idxOut;
			float m;

			//Remove this clip plane from remaining to process
			cpBitsRemain -= cpBit;

			lineDot[0] = Dot3(clPlane.n, clLine.pts[0]);
			lineDot[1] = Dot3(clPlane.n, clLine.pts[1]);
			lineIn[0] = (lineDot[0] >= -clPlane.d) ? true : false;
			lineIn[1] = (lineDot[1] >= -clPlane.d) ? true : false;

			//Next clip plane if both inside
			if (lineIn[0] & lineIn[1]) {
				continue;
			}

			//Discard line if entirely clipped
			if (!(lineIn[0] | lineIn[1])) {
				clLine.numPts = 0;
				break;
			}

			//Otherwise, need to clip
			idxIn = (lineIn[0]) ? 0 : 1;
			idxOut = (!lineIn[0]) ? 0 : 1;

			m = (-clPlane.d - lineDot[idxIn]) / (lineDot[idxOut] - lineDot[idxIn]);
			clLine.pts[idxOut].x = clLine.pts[idxIn].x + (m * (clLine.pts[idxOut].x - clLine.pts[idxIn].x));
			clLine.pts[idxOut].y = clLine.pts[idxIn].y + (m * (clLine.pts[idxOut].y - clLine.pts[idxIn].y));
			clLine.pts[idxOut].z = clLine.pts[idxIn].z + (m * (clLine.pts[idxOut].z - clLine.pts[idxIn].z));
		}
	}

	return;
}

void CGClip::ClipTri(cl_tri_t &clTri) {
	unsigned int cpBitsRemain;
	unsigned int cpBit;
	unsigned int cpNum;

	//Not supposed to clip an already clipped triangle
	//Also reject invalid too small number of points early
	if (clTri.numPts != 3) {
		return;
	}

	//Clip to all enabled planes
	cpBitsRemain = m_cpEnableBits;
	for (cpNum = 0, cpBit = 0x1; cpBitsRemain != 0; cpNum++, cpBit <<= 1) {
		//Clip to plane if enabled
		if (cpBit & cpBitsRemain) {
			const plane_t &clPlane = m_cp[cpNum];
			float prevDot;
			bool prevIn;
			unsigned int ptNum;
			unsigned int numPtsOut;
			vec3_t ptsOut[3 + NUM_CP];

			//Remove this clip plane from remaining to process
			cpBitsRemain -= cpBit;

			//Clip all points in currently poly to plane
			prevDot = Dot3(clPlane.n, clTri.pts[clTri.numPts - 1]);
			prevIn = (prevDot >= -clPlane.d) ? true : false;
			numPtsOut = 0;
			for (ptNum = 0; ptNum < clTri.numPts; ptNum++) {
				float curDot;
				bool curIn;

				curDot = Dot3(clPlane.n, clTri.pts[ptNum]);
				curIn = (curDot >= -clPlane.d) ? true : false;

				//Add new clipped vertex if previous and current cross the clip plane
				if (prevIn != curIn) {
					unsigned int idxIn, idxOut;
					float dotIn, dotOut;
					float m;
					const vec3_t *pPtIn, *pPtOut;

					if (curIn) {
						idxIn = ptNum;
						idxOut = (ptNum == 0) ? (clTri.numPts - 1) : (ptNum - 1);
						dotIn = curDot;
						dotOut = prevDot;
					}
					else {
						idxOut = ptNum;
						idxIn = (ptNum == 0) ? (clTri.numPts - 1) : (ptNum - 1);
						dotOut = curDot;
						dotIn = prevDot;
					}

					m = (-clPlane.d - dotIn) / (dotOut - dotIn);
					pPtIn = &clTri.pts[idxIn];
					pPtOut = &clTri.pts[idxOut];
					ptsOut[numPtsOut].x = pPtIn->x + (m * (pPtOut->x - pPtIn->x));
					ptsOut[numPtsOut].y = pPtIn->y + (m * (pPtOut->y - pPtIn->y));
					ptsOut[numPtsOut].z = pPtIn->z + (m * (pPtOut->z - pPtIn->z));
					numPtsOut++;
				}

				//If current vertex in, add to list
				if (curIn) {
					ptsOut[numPtsOut] = clTri.pts[ptNum];
					numPtsOut++;
				}

				//Move to next
				prevDot = curDot;
				prevIn = curIn;
			}

			//Copy back to primary input / output clipped triangle structure
			//If entirely clipped, mark as zero points and return
			if (numPtsOut < 3) {
				clTri.numPts = 0;
				break;
			}
			clTri.numPts = numPtsOut;
			for (ptNum = 0; ptNum < numPtsOut; ptNum++) {
				clTri.pts[ptNum] = ptsOut[ptNum];
			}
		}
	}

	return;
}


void CGClip::SelectModeStart(void) {
	//Make sure hit name stack starts out clear
	ClearHitNameStack();

	//Reset best depth value to farthest
	m_selClosestDepth = std::numeric_limits<float>::infinity();

	//Initial reset hit flag
	m_selHit = false;

	return;
}

void CGClip::SelectModeEnd(void) {
	//Clear hit name stack when done
	ClearHitNameStack();

	return;
}


bool CGClip::CheckNewSelectHit(void) {
	return m_selHit;
}

void CGClip::ClearHitNameStack(void) {
	//Clear
	m_hitNameStack.clear();

	return;
}

void CGClip::PushHitName(unsigned int hitName) {
	//Add to end
	m_hitNameStack.push_back(hitName);

	//Reset hit flag
	m_selHit = false;

	return;
}

void CGClip::PopHitName(void) {
	//Remove from end if not empty
	if (!m_hitNameStack.empty()) {
		m_hitNameStack.pop_back();
	}

	//Reset hit flag
	m_selHit = false;

	return;
}

size_t CGClip::GetHitNameStackSize(void) {
	return m_hitNameStack.size();
}

void CGClip::GetHitNameStackValues(unsigned int *pDst, size_t dstSize) {
	size_t numNames;
	unsigned int u;
	std::deque<unsigned int>::const_iterator hnIter;

	numNames = m_hitNameStack.size();
	if (numNames > dstSize) {
		numNames = dstSize;
	}

	hnIter = m_hitNameStack.begin();
	for (u = 0; u < numNames; u++) {
		*pDst++ = *hnIter++;
	}

	return;
}


void CGClip::SelectDrawLine(const vec3_t *pLnPts) {
	cl_line_t clLn;
	unsigned int u;

	//Discard if hit name stack empty
	if (m_hitNameStack.empty()) {
		return;
	}

	clLn.numPts = 2;
	clLn.pts[0] = pLnPts[0];
	clLn.pts[1] = pLnPts[1];

	ClipLine(clLn);

	for (u = 0; u < clLn.numPts; u++) {
		float ptZ = clLn.pts[u].z;

		if (ptZ <= m_selClosestDepth) {
			m_selClosestDepth = ptZ;
			m_selHit = true;
		}
	}

	return;
}

void CGClip::SelectDrawTri(const vec3_t *pTriPts) {
	cl_tri_t clTri;
	unsigned int u;

	//Discard if hit name stack empty
	if (m_hitNameStack.empty()) {
		return;
	}

	clTri.numPts = 3;
	clTri.pts[0] = pTriPts[0];
	clTri.pts[1] = pTriPts[1];
	clTri.pts[2] = pTriPts[2];

	ClipTri(clTri);

	for (u = 0; u < clTri.numPts; u++) {
		float ptZ = clTri.pts[u].z;

		if (ptZ <= m_selClosestDepth) {
			m_selClosestDepth = ptZ;
			m_selHit = true;
		}
	}

	return;
}

