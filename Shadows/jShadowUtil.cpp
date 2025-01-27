﻿#include "pch.h"
#include "jShadowVolume.h"
#include "jObject.h"
#include "Math/Matrix.h"
#include "jRenderObject.h"
#include "jVertexAdjacency.h"
#include "jRHI.h"
#include "jPrimitiveUtil.h"

void jShadowVolume::Update(const Vector* lightDirection, const Vector* lightPos, jObject* ownerObject)
{
	QuadVertices.clear();
	EdgeVertices.clear();
	Edges.clear();

	const Matrix matWorldInv = ownerObject->RenderObject->World.GetInverse();
	Vector lightDirWorldInv(Vector::ZeroVector);
	if (lightDirection)
		lightDirWorldInv = Matrix3(matWorldInv).Transform(*lightDirection);
	Vector lightPosWorldInv(Vector::ZeroVector);
	if (lightPos)
		lightPosWorldInv = matWorldInv.Transform(*lightPos);

	const auto getLightDirection = [&lightDirection, &lightDirWorldInv, &lightPos, &lightPosWorldInv](const Vector& pos)
	{
		if (lightDirection)
			return lightDirWorldInv;

		if (lightPos)
			return pos - lightPosWorldInv;

		JMESSAGE("lightDirection or lightPos should be not null");
		return Vector::ZeroVector;
	};

	std::map<int32, bool> backfaceToLightDirInTriangle;
	for (auto& iter : VertexAdjacency->Triangles)
	{
		const auto& key = iter.first;
		const auto triangle = iter.second;
		auto v0 = VertexAdjacency->Vertices[triangle->VertexIndex0];
		auto v1 = VertexAdjacency->Vertices[triangle->VertexIndex1];
		auto v2 = VertexAdjacency->Vertices[triangle->VertexIndex2];

		auto lightDir = getLightDirection(triangle->CenterPos);
		const auto isBackfaceToLight = (triangle->Normal.DotProduct(lightDir) > 0.0);
		backfaceToLightDirInTriangle[triangle->TriangleIndex] = isBackfaceToLight;

		bool needFrontCap = false;
		bool needBackCap = false;
		if (ownerObject->RenderObject->IsTwoSided)
		{
			needFrontCap = true;
			needBackCap = true;
		}
		else
		{
			if (!isBackfaceToLight)
				needFrontCap = true;
			else
				needBackCap = true;
		}

		if (needFrontCap)
		{
			UpdateEdge(triangle->EdgeKey0);
			UpdateEdge(triangle->EdgeKey1);
			UpdateEdge(triangle->EdgeKey2);

			// front cap
			if (isBackfaceToLight)
			{
				auto temp = v2;
				v2 = v1;
				v1 = temp;
			}
			QuadVertices.push_back(v0.x);   QuadVertices.push_back(v0.y);   QuadVertices.push_back(v0.z);    QuadVertices.push_back(1.0);
			QuadVertices.push_back(v1.x);   QuadVertices.push_back(v1.y);   QuadVertices.push_back(v1.z);    QuadVertices.push_back(1.0);
			QuadVertices.push_back(v2.x);   QuadVertices.push_back(v2.y);   QuadVertices.push_back(v2.z);    QuadVertices.push_back(1.0);
		}

		if (needBackCap)
		{
			// back cap
			if (ownerObject->RenderObject->IsTwoSided || !isBackfaceToLight)
			{
				auto temp = v2;
				v2 = v1;
				v1 = temp;
			}

			QuadVertices.push_back(v0.x);   QuadVertices.push_back(v0.y);   QuadVertices.push_back(v0.z);    QuadVertices.push_back(0.0);
			QuadVertices.push_back(v1.x);   QuadVertices.push_back(v1.y);   QuadVertices.push_back(v1.z);    QuadVertices.push_back(0.0);
			QuadVertices.push_back(v2.x);   QuadVertices.push_back(v2.y);   QuadVertices.push_back(v2.z);    QuadVertices.push_back(0.0);
		}
	}

	for (auto& iter : Edges)
	{
		const auto edge = VertexAdjacency->Edges[iter];
		const auto v0 = VertexAdjacency->Vertices[edge->VertexIndex0];
		const auto v1 = VertexAdjacency->Vertices[edge->VertexIndex1];

		EdgeVertices.push_back(v0.x);
		EdgeVertices.push_back(v0.y);
		EdgeVertices.push_back(v0.z);
		EdgeVertices.push_back(v1.x);
		EdgeVertices.push_back(v1.y);
		EdgeVertices.push_back(v1.z);
	}
	/////////////////////////////////////////

	// 생성된 Edge로 Quad 생성
	for (auto& iter : Edges)
	{
		const auto edge = VertexAdjacency->Edges[iter];
		const auto triangle = VertexAdjacency->Triangles[edge->TriangleIndex];
		auto lightDir = getLightDirection(triangle->CenterPos);

		auto v0 = VertexAdjacency->Vertices[edge->VertexIndex0];
		auto v1 = VertexAdjacency->Vertices[edge->VertexIndex1];
		auto v2 = v0;
		auto v3 = v1;

		// quad should face to triangle normal
		const auto isBackfaceToLight = backfaceToLightDirInTriangle[triangle->TriangleIndex];

		if (isBackfaceToLight)
		{
			{
				auto temp = v0;
				v0 = v1;
				v1 = temp;
			}

			{
				auto temp = v2;
				v2 = v3;
				v3 = temp;
			}
		}

		QuadVertices.push_back(v0.x);   QuadVertices.push_back(v0.y);   QuadVertices.push_back(v0.z);    QuadVertices.push_back(1.0);
		QuadVertices.push_back(v1.x);   QuadVertices.push_back(v1.y);   QuadVertices.push_back(v1.z);    QuadVertices.push_back(1.0);
		QuadVertices.push_back(v2.x);   QuadVertices.push_back(v2.y);   QuadVertices.push_back(v2.z);    QuadVertices.push_back(0.0);

		QuadVertices.push_back(v2.x);   QuadVertices.push_back(v2.y);   QuadVertices.push_back(v2.z);    QuadVertices.push_back(0.0);
		QuadVertices.push_back(v1.x);   QuadVertices.push_back(v1.y);   QuadVertices.push_back(v1.z);    QuadVertices.push_back(1.0);
		QuadVertices.push_back(v3.x);   QuadVertices.push_back(v3.y);   QuadVertices.push_back(v3.z);    QuadVertices.push_back(0.0);
	}
	/////////////////////////////////////////

	if (IsInitialized)
	{
		UpdateShadowVolumeObject();
	}
	else
	{
		CreateShadowVolumeObject();
		IsInitialized = true;
	}

	EdgeObject->RenderObject->Pos = ownerObject->RenderObject->Pos;
	EdgeObject->RenderObject->Rot = ownerObject->RenderObject->Rot;
	EdgeObject->RenderObject->Scale = ownerObject->RenderObject->Scale;

	QuadObject->RenderObject->Pos = ownerObject->RenderObject->Pos;
	QuadObject->RenderObject->Rot = ownerObject->RenderObject->Rot;
	QuadObject->RenderObject->Scale = ownerObject->RenderObject->Scale;
}

void jShadowVolume::UpdateEdge(size_t edgeKey) // 에지 검출
{
	if (Edges.end() != Edges.find(edgeKey))
		Edges.erase(edgeKey);
	else
		Edges.insert(edgeKey);
}

void jShadowVolume::CreateShadowVolumeObject()
{
	if (CreateEdgeObject)
	{
		const int32 elementCount = static_cast<int32>(EdgeVertices.size() / 3);

		// attribute 추가
		EdgeVertexStreamData = std::shared_ptr<jVertexStreamData>(new jVertexStreamData());

		{
			auto streamParam = new jStreamParam<float>();
			streamParam->BufferType = EBufferType::STATIC;
			streamParam->ElementTypeSize = sizeof(float);
			streamParam->ElementType = EBufferElementType::FLOAT;
			streamParam->Stride = sizeof(float) * 3;
			streamParam->Name = "Pos";
			streamParam->Data.resize(EdgeVertices.size());
			memcpy(&streamParam->Data[0], &EdgeVertices[0], EdgeVertices.size() * sizeof(float));
			EdgeVertexStreamData->Params.push_back(streamParam);
		}

		{
			auto streamParam = new jStreamParam<float>();
			streamParam->BufferType = EBufferType::STATIC;
			streamParam->ElementType = EBufferElementType::FLOAT;
			streamParam->ElementTypeSize = sizeof(float);
			streamParam->Stride = sizeof(float) * 4;
			streamParam->Name = "Color";
			streamParam->Data = std::move(jPrimitiveUtil::GenerateColor(Vector4(0.0f, 0.0f, 1.0f, 1.0f), elementCount));
			EdgeVertexStreamData->Params.push_back(streamParam);
		}

		EdgeVertexStreamData->PrimitiveType = EPrimitiveType::LINES;
		EdgeVertexStreamData->ElementCount = elementCount;

		auto object = new jObject();
		auto renderObject = new jRenderObject();
		renderObject->CreateRenderObject(EdgeVertexStreamData, nullptr);
		object->RenderObject = renderObject;
		EdgeObject = object;
	}

	if (CreateQuadObject)
	{
		const int32 elementCount = static_cast<int32>(QuadVertices.size() / 4);

		// attribute 추가
		QuadVertexStreamData = std::shared_ptr<jVertexStreamData>(new jVertexStreamData());

		{
			auto streamParam = new jStreamParam<float>();
			streamParam->BufferType = EBufferType::STATIC;
			streamParam->ElementTypeSize = sizeof(float);
			streamParam->ElementType = EBufferElementType::FLOAT;
			streamParam->Stride = sizeof(float) * 4;
			streamParam->Name = "Pos";
			streamParam->Data.resize(QuadVertices.size());
			memcpy(&streamParam->Data[0], &QuadVertices[0], QuadVertices.size() * sizeof(float));
			QuadVertexStreamData->Params.push_back(streamParam);
		}

		{
			auto streamParam = new jStreamParam<float>();
			streamParam->BufferType = EBufferType::STATIC;
			streamParam->ElementType = EBufferElementType::FLOAT;
			streamParam->ElementTypeSize = sizeof(float);
			streamParam->Stride = sizeof(float) * 4;
			streamParam->Name = "Color";
			streamParam->Data = std::move(jPrimitiveUtil::GenerateColor(Vector4(0.0f, 0.0f, 1.0f, 1.0f), elementCount));
			QuadVertexStreamData->Params.push_back(streamParam);
		}

		QuadVertexStreamData->PrimitiveType = EPrimitiveType::TRIANGLES;
		QuadVertexStreamData->ElementCount = elementCount;

		auto object = new jObject();
		auto renderObject = new jRenderObject();
		renderObject->CreateRenderObject(QuadVertexStreamData, nullptr);
		object->RenderObject = renderObject;
		QuadObject = object;
	}
}

void jShadowVolume::UpdateShadowVolumeObject()
{
	if (CreateEdgeObject)
	{
		const int32 elementCount = static_cast<int32>(EdgeVertices.size() / 3);

		// attribute 추가
		EdgeVertexStreamData = std::shared_ptr<jVertexStreamData>(new jVertexStreamData());

		{
			auto streamParam = new jStreamParam<float>();
			streamParam->BufferType = EBufferType::STATIC;
			streamParam->ElementTypeSize = sizeof(float);
			streamParam->ElementType = EBufferElementType::FLOAT;
			streamParam->Stride = sizeof(float) * 3;
			streamParam->Name = "Pos";
			streamParam->Data.resize(EdgeVertices.size());
			memcpy(&streamParam->Data[0], &EdgeVertices[0], EdgeVertices.size() * sizeof(float));
			EdgeVertexStreamData->Params.push_back(streamParam);
		}

		{
			auto streamParam = new jStreamParam<float>();
			streamParam->BufferType = EBufferType::STATIC;
			streamParam->ElementType = EBufferElementType::FLOAT;
			streamParam->ElementTypeSize = sizeof(float);
			streamParam->Stride = sizeof(float) * 4;
			streamParam->Name = "Color";
			streamParam->Data = std::move(jPrimitiveUtil::GenerateColor(Vector4(0.0f, 0.0f, 1.0f, 1.0f), elementCount));
			EdgeVertexStreamData->Params.push_back(streamParam);
		}

		EdgeVertexStreamData->PrimitiveType = EPrimitiveType::LINES;
		EdgeVertexStreamData->ElementCount = elementCount;

		g_rhi->UpdateVertexBuffer(EdgeObject->RenderObject->VertexBuffer, EdgeVertexStreamData);
	}

	if (CreateQuadObject)
	{
		const int32 elementCount = static_cast<int32>(QuadVertices.size() / 4);

		// attribute 추가
		QuadVertexStreamData = std::shared_ptr<jVertexStreamData>(new jVertexStreamData());

		{
			auto streamParam = new jStreamParam<float>();
			streamParam->BufferType = EBufferType::STATIC;
			streamParam->ElementTypeSize = sizeof(float);
			streamParam->ElementType = EBufferElementType::FLOAT;
			streamParam->Stride = sizeof(float) * 4;
			streamParam->Name = "Pos";
			streamParam->Data.resize(QuadVertices.size());
			memcpy(&streamParam->Data[0], &QuadVertices[0], QuadVertices.size() * sizeof(float));
			QuadVertexStreamData->Params.push_back(streamParam);
		}

		{
			auto streamParam = new jStreamParam<float>();
			streamParam->BufferType = EBufferType::STATIC;
			streamParam->ElementType = EBufferElementType::FLOAT;
			streamParam->ElementTypeSize = sizeof(float);
			streamParam->Stride = sizeof(float) * 4;
			streamParam->Name = "Color";
			streamParam->Data = std::move(jPrimitiveUtil::GenerateColor(Vector4(0.0f, 0.0f, 1.0f, 1.0f), elementCount));
			QuadVertexStreamData->Params.push_back(streamParam);
		}

		QuadVertexStreamData->PrimitiveType = EPrimitiveType::TRIANGLES;
		QuadVertexStreamData->ElementCount = elementCount;

		g_rhi->UpdateVertexBuffer(QuadObject->RenderObject->VertexBuffer, QuadVertexStreamData);
	}
}
