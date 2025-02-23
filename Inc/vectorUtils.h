#pragma once

#include <DirectXMath.h>

inline DirectX::XMMATRIX ToXMMATRIX(const D3DMATRIX& d3dMatrix) {
	D3DMATRIX temp = d3dMatrix;
	return reinterpret_cast<DirectX::XMMATRIX&>(temp);
}

inline D3DMATRIX ToD3DMATRIX(const DirectX::XMMATRIX& xMMatrix) {
	DirectX::XMMATRIX temp = xMMatrix;
	return reinterpret_cast<D3DMATRIX&>(temp);
}

static const D3DMATRIX identityMatrix = ToD3DMATRIX(DirectX::XMMatrixIdentity());

inline DirectX::XMVECTOR FVecToDXVec(const FVector& vec) {
	return DirectX::XMVectorSet(vec.X, vec.Y, vec.Z, 0.0f);
}

inline FVector DXVecToFVec(const DirectX::XMVECTOR& vec) {
	using namespace DirectX;
	return FVector(XMVectorGetX(vec), XMVectorGetY(vec), XMVectorGetZ(vec));
}

inline DirectX::XMMATRIX FCoordToDXMat(const FCoords& coord) {
	using namespace DirectX;
	XMMATRIX mat = {
		coord.XAxis.X, coord.XAxis.Y, coord.XAxis.Z, 0,
		coord.YAxis.X, coord.YAxis.Y, coord.YAxis.Z, 0,
		coord.ZAxis.X, coord.ZAxis.Y, coord.ZAxis.Z, 0,
		coord.Origin.X, coord.Origin.Y, coord.Origin.Z, 1.0f
	};
	return mat;
}

inline DirectX::XMMATRIX FRotToDXRotMat(const FRotator& rot) {
	using namespace DirectX;
	FCoords coords = GMath.UnitCoords;
	coords /= rot;
	return FCoordToDXMat(coords);
}

inline FCoords DXMatToFCoord(const DirectX::XMMATRIX& mat) {
	using namespace DirectX;
	FCoords coords(
		DXVecToFVec(mat.r[3]),
		DXVecToFVec(mat.r[0]),
		DXVecToFVec(mat.r[1]),
		DXVecToFVec(mat.r[2])
	);
	return coords;
}
