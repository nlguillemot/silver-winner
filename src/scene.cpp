#include "scene.h"

#include "apputil.h"
#include "renderer.h"
#include "app.h"
#include "flythrough_camera.h"

#include "imgui.h"
#include "tiny_obj_loader.h"

#include <DirectXMath.h>

using namespace DirectX;

using float4 = XMFLOAT4;
using float4x4 = XMFLOAT4X4;

#include "shaders/common.hlsl"

struct VertexPosition
{
    XMFLOAT3 Position;
};

struct VertexTexCoord
{
    XMFLOAT2 TexCoord;
};

struct VertexNormal
{
    XMFLOAT3 Normal;
};

struct Material
{
    std::string Name;
    XMFLOAT3 Ambient;
    XMFLOAT3 Diffuse;
    XMFLOAT3 Specular;
    float Shininess;
    float Opacity;
};

struct StaticMesh
{
    std::string Name;
    ComPtr<ID3D11Buffer> pPositionVertexBuffer;
    ComPtr<ID3D11Buffer> pTexCoordVertexBuffer;
    ComPtr<ID3D11Buffer> pNormalVertexBuffer;
    ComPtr<ID3D11Buffer> pIndexBuffer;
    int MaterialID; // the material this mesh was designed for
    UINT IndexCountPerInstance;
    UINT StartIndexLocation;
};

struct NodeTransform
{
    XMVECTOR Scale;
    XMVECTOR Quaternion;
    XMVECTOR Translation;
};

struct StaticMeshNode
{
    int StaticMeshID;
};

enum SceneNodeType
{
    SCENENODETYPE_STATICMESH
};

struct SceneNode
{
    NodeTransform Transform;

    int MaterialID;
    
    SceneNodeType Type;

    union
    {
        StaticMeshNode AsStaticMesh;
    };
};

struct Scene
{
    std::vector<Material> Materials;
    std::vector<StaticMesh> StaticMeshes;
    std::vector<SceneNode> SceneNodes;

    D3D11_VIEWPORT SceneViewport;

    ComPtr<ID3D11Texture2D> pSceneDepthTex2D;
    ComPtr<ID3D11DepthStencilView> pSceneDepthDSV;

    XMFLOAT3 CameraPos;
    XMFLOAT3 CameraLook;
    ComPtr<ID3D11Buffer> pCameraBuffer;
    ComPtr<ID3D11Buffer> pMaterialBuffer;
    ComPtr<ID3D11Buffer> pSceneNodeBuffer;

    ComPtr<ID3D11InputLayout> pSceneInputLayout;
    ComPtr<ID3D11RasterizerState> pSceneRasterizerState;
    ComPtr<ID3D11DepthStencilState> pSceneDepthStencilState;
    ComPtr<ID3D11BlendState> pSceneBlendState;

    Shader* SceneVS;
    Shader* ScenePS;

    uint64_t LastTicks;
    int LastMouseX, LastMouseY;
};

Scene g_Scene;

static void SceneAddObjMesh(
    const char* filename, const char* mtlbasepath,
    std::vector<int>* newStaticMeshIDs = NULL,
    std::vector<int>* newMaterialIDs = NULL)
{
    ID3D11Device* dev = RendererGetDevice();

    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;
    if (!tinyobj::LoadObj(shapes, materials, err, filename, mtlbasepath))
    {
        SimpleMessageBox_FatalError("Failed to load mesh: %s\nReason: %s", filename, err.c_str());
    }

    int firstMaterial = (int)g_Scene.Materials.size();

    for (tinyobj::material_t& material : materials)
    {
        Material m;
        m.Name = material.name;
        m.Ambient.x = material.ambient[0];
        m.Ambient.y = material.ambient[1];
        m.Ambient.z = material.ambient[2];
        m.Diffuse.x = material.diffuse[0];
        m.Diffuse.y = material.diffuse[1];
        m.Diffuse.z = material.diffuse[2];
        m.Specular.x = material.specular[0];
        m.Specular.y = material.specular[1];
        m.Specular.z = material.specular[2];
        m.Shininess = material.shininess;
        m.Opacity = material.dissolve;

        if (newMaterialIDs)
            newMaterialIDs->push_back((int)g_Scene.Materials.size());

        g_Scene.Materials.push_back(std::move(m));
    }

    for (tinyobj::shape_t& shape : shapes)
    {
        tinyobj::mesh_t& mesh = shape.mesh;

        if (mesh.positions.size() % 3 != 0)
        {
            SimpleMessageBox_FatalError("Meshes must use 3D positions");
        }

        ComPtr<ID3D11Buffer> pPositionBuffer;
        ComPtr<ID3D11Buffer> pTexCoordBuffer;
        ComPtr<ID3D11Buffer> pNormalBuffer;
        ComPtr<ID3D11Buffer> pIndexBuffer;

        UINT numVertices = (UINT)mesh.positions.size() / 3;

        if (!mesh.positions.empty())
        {
            D3D11_BUFFER_DESC positionVertexBufferDesc = {};
            positionVertexBufferDesc.ByteWidth = sizeof(VertexPosition) * numVertices;
            positionVertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            positionVertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

            D3D11_SUBRESOURCE_DATA positionVertexBufferData = {};
            positionVertexBufferData.pSysMem = mesh.positions.data();

            CHECKHR(dev->CreateBuffer(&positionVertexBufferDesc, &positionVertexBufferData, &pPositionBuffer));
        }

        if (!mesh.texcoords.empty())
        {
            D3D11_BUFFER_DESC texCoordVertexBufferDesc = {};
            texCoordVertexBufferDesc.ByteWidth = sizeof(VertexTexCoord) * numVertices;
            texCoordVertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            texCoordVertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

            D3D11_SUBRESOURCE_DATA texcoordVertexBufferData = {};
            if (mesh.texcoords.size() == numVertices * 2)
            {
                texcoordVertexBufferData.pSysMem = mesh.texcoords.data();
            }
            else
            {
                SimpleMessageBox_FatalError("TexCoord conversion required (Expected 2D, got %dD)", mesh.texcoords.size() / numVertices);
            }

            CHECKHR(dev->CreateBuffer(&texCoordVertexBufferDesc, &texcoordVertexBufferData, &pTexCoordBuffer));
        }

        if (!mesh.normals.empty())
        {
            D3D11_BUFFER_DESC normalVertexBufferDesc = {};
            normalVertexBufferDesc.ByteWidth = sizeof(VertexNormal) * numVertices;
            normalVertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            normalVertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

            D3D11_SUBRESOURCE_DATA normalVertexBufferData = {};
            if (mesh.normals.size() == numVertices * 3)
            {
                normalVertexBufferData.pSysMem = mesh.normals.data();
            }
            else
            {
                SimpleMessageBox_FatalError("Normal conversion required (Expected 3D, got %dD)", mesh.normals.size() / numVertices);
            }

            CHECKHR(dev->CreateBuffer(&normalVertexBufferDesc, &normalVertexBufferData, &pNormalBuffer));
        }

        UINT numIndices = (UINT)mesh.indices.size();

        if (!mesh.indices.empty())
        {
            D3D11_BUFFER_DESC indexBufferDesc = {};
            indexBufferDesc.ByteWidth = sizeof(UINT32) * numIndices;
            indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

            D3D11_SUBRESOURCE_DATA indexBufferData = {};
            static_assert(sizeof(mesh.indices[0]) == sizeof(UINT32), "Expecting UINT32 indices");
            indexBufferData.pSysMem = mesh.indices.data();

            CHECKHR(dev->CreateBuffer(&indexBufferDesc, &indexBufferData, &pIndexBuffer));
        }

        int firstFace = 0;

        const int numFaces = (int)shape.mesh.indices.size() / 3;
        for (int face = 0; face < numFaces; face++)
        {
            int currMTL = shape.mesh.material_ids[face];
            
            int nextMTL = -1;
            if (face + 1 < numFaces)
                nextMTL = shape.mesh.material_ids[face + 1];
            
            if (currMTL == nextMTL)
            {
                // still same material, don't need to output mesh yet
                continue;
            }

            StaticMesh sm;
            sm.Name = shape.name;
            sm.pPositionVertexBuffer = pPositionBuffer;
            sm.pTexCoordVertexBuffer = pTexCoordBuffer;
            sm.pNormalVertexBuffer = pNormalBuffer;
            sm.pIndexBuffer = pIndexBuffer;
            sm.MaterialID = firstMaterial + currMTL;
            sm.IndexCountPerInstance = (face + 1 - firstFace) * 3;
            sm.StartIndexLocation = firstFace * 3;

            if (newStaticMeshIDs)
                newStaticMeshIDs->push_back((int)g_Scene.StaticMeshes.size());

            g_Scene.StaticMeshes.push_back(std::move(sm));

            // first face for next mesh
            firstFace = face + 1;
        }
    }
}

static int SceneAddStaticMeshSceneNode(int staticMeshID)
{
    const StaticMesh& staticMesh = g_Scene.StaticMeshes[staticMeshID];

    SceneNode sceneNode;
    sceneNode.Transform.Scale = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
    sceneNode.Transform.Quaternion = XMQuaternionIdentity();
    sceneNode.Transform.Translation = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    sceneNode.MaterialID = staticMesh.MaterialID;
    sceneNode.Type = SCENENODETYPE_STATICMESH;
    sceneNode.AsStaticMesh.StaticMeshID = staticMeshID;

    g_Scene.SceneNodes.push_back(std::move(sceneNode));
    return (int)g_Scene.SceneNodes.size() - 1;
}

void SceneInit()
{
    ID3D11Device* dev = RendererGetDevice();

    std::vector<std::string> meshesToLoad = {
        "sponza",
        "cube"
    };

    std::vector<int> newStaticMeshIDs;
    for (const std::string& meshToLoad : meshesToLoad)
    {
        std::string meshFolder = "assets/" + meshToLoad + "/";
        std::string meshFile = meshFolder + meshToLoad + ".obj";
        SceneAddObjMesh(meshFile.c_str(), meshFolder.c_str(), &newStaticMeshIDs);
    }

    int cubeSceneNodeID = -1;
    for (int newStaticMeshID : newStaticMeshIDs)
    {
        int sceneNodeID = SceneAddStaticMeshSceneNode(newStaticMeshID);

        if (g_Scene.StaticMeshes[newStaticMeshID].Name == "cube")
        {
            cubeSceneNodeID = sceneNodeID;
        }
    }

    if (cubeSceneNodeID != -1)
    {
        g_Scene.SceneNodes[cubeSceneNodeID].Transform.Scale = XMVectorSet(100.0f, 100.0f, 100.0f, 0.0f);
        g_Scene.SceneNodes[cubeSceneNodeID].Transform.Quaternion = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(30.0f));
        g_Scene.SceneNodes[cubeSceneNodeID].Transform.Translation = XMVectorSet(200.0f, 50.0f, 0.0f, 1.0f);
    }

    g_Scene.SceneVS = RendererAddShader("scene.hlsl", "VSmain", "vs_5_0");
    g_Scene.ScenePS = RendererAddShader("scene.hlsl", "PSmain", "ps_5_0");

    XMStoreFloat3(&g_Scene.CameraPos, XMVectorSet(0.0f, 200.0f, 0.0f, 1.0f));
    XMStoreFloat3(&g_Scene.CameraLook, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));

    D3D11_BUFFER_DESC cameraBufferDesc = {};
    cameraBufferDesc.ByteWidth = sizeof(PerCameraData);
    cameraBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    cameraBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cameraBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    CHECKHR(dev->CreateBuffer(&cameraBufferDesc, NULL, &g_Scene.pCameraBuffer));

    D3D11_BUFFER_DESC materialBufferDesc = {};
    materialBufferDesc.ByteWidth = sizeof(PerMaterialData);
    materialBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    materialBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    materialBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    CHECKHR(dev->CreateBuffer(&materialBufferDesc, NULL, &g_Scene.pMaterialBuffer));

    D3D11_BUFFER_DESC sceneNodeBufferDesc = {};
    sceneNodeBufferDesc.ByteWidth = sizeof(PerSceneNodeData);
    sceneNodeBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    sceneNodeBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    sceneNodeBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    CHECKHR(dev->CreateBuffer(&sceneNodeBufferDesc, NULL, &g_Scene.pSceneNodeBuffer));

    D3D11_INPUT_ELEMENT_DESC sceneInputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    CHECKHR(dev->CreateInputLayout(
        sceneInputElements, _countof(sceneInputElements),
        g_Scene.SceneVS->Blob->GetBufferPointer(), g_Scene.SceneVS->Blob->GetBufferSize(),
        &g_Scene.pSceneInputLayout));

    D3D11_RASTERIZER_DESC sceneRasterizerDesc = {};
    sceneRasterizerDesc.FillMode = D3D11_FILL_SOLID;
    sceneRasterizerDesc.CullMode = D3D11_CULL_NONE;
    sceneRasterizerDesc.FrontCounterClockwise = FALSE;
    sceneRasterizerDesc.DepthClipEnable = TRUE;
    CHECKHR(dev->CreateRasterizerState(&sceneRasterizerDesc, &g_Scene.pSceneRasterizerState));

    D3D11_DEPTH_STENCIL_DESC sceneDepthStencilDesc = {};
    sceneDepthStencilDesc.DepthEnable = TRUE;
    sceneDepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    sceneDepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    CHECKHR(dev->CreateDepthStencilState(&sceneDepthStencilDesc, &g_Scene.pSceneDepthStencilState));

    D3D11_BLEND_DESC sceneBlendDesc = {};
    sceneBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    CHECKHR(dev->CreateBlendState(&sceneBlendDesc, &g_Scene.pSceneBlendState));

    g_Scene.LastMouseX = INT_MIN;
    g_Scene.LastMouseY = INT_MIN;
}

void SceneResize(
    int windowWidth, int windowHeight,
    int renderWidth, int renderHeight)
{
    ID3D11Device* dev = RendererGetDevice();

    D3D11_TEXTURE2D_DESC sceneDepthTex2DDesc = {};
    sceneDepthTex2DDesc.Width = renderWidth;
    sceneDepthTex2DDesc.Height = renderHeight;
    sceneDepthTex2DDesc.MipLevels = 1;
    sceneDepthTex2DDesc.ArraySize = 1;
    sceneDepthTex2DDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    sceneDepthTex2DDesc.SampleDesc.Count = 1;
    sceneDepthTex2DDesc.Usage = D3D11_USAGE_DEFAULT;
    sceneDepthTex2DDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> pSceneDepthTex2D;
    CHECKHR(dev->CreateTexture2D(&sceneDepthTex2DDesc, NULL, &pSceneDepthTex2D));

    D3D11_DEPTH_STENCIL_VIEW_DESC sceneDepthDSVDesc = {};
    sceneDepthDSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
    sceneDepthDSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11DepthStencilView> pSceneDepthDSV;
    CHECKHR(dev->CreateDepthStencilView(pSceneDepthTex2D.Get(), &sceneDepthDSVDesc, &pSceneDepthDSV));

    g_Scene.SceneViewport = D3D11_VIEWPORT{ 0.0f, 0.0f, (FLOAT)renderWidth, (FLOAT)renderHeight, 0.0f, 1.0f };

    g_Scene.pSceneDepthTex2D.Swap(pSceneDepthTex2D);
    g_Scene.pSceneDepthDSV.Swap(pSceneDepthDSV);
}

void ScenePaint(ID3D11RenderTargetView* pBackBufferRTV)
{
    uint64_t currTicks;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTicks);
    
    if (g_Scene.LastTicks == 0) {
        g_Scene.LastTicks = currTicks;
    }
    
    uint64_t deltaTicks = currTicks - g_Scene.LastTicks;

    uint64_t ticksPerSecond;
    QueryPerformanceFrequency((LARGE_INTEGER*)&ticksPerSecond);

    int currMouseX, currMouseY;
    AppGetClientCursorPos(&currMouseX, &currMouseY);
    
    // Initialize the last mouse position on the first update
    if (g_Scene.LastMouseX == INT_MIN)
        g_Scene.LastMouseX = currMouseX;
    if (g_Scene.LastMouseY == INT_MIN)
        g_Scene.LastMouseY = currMouseY;

    ID3D11Device* dev = RendererGetDevice();
    ID3D11DeviceContext* dc = RendererGetDeviceContext();

    // Update camera
    {
        float activated = GetAsyncKeyState(VK_RBUTTON) ? 1.0f : 0.0f;
        float up[3] = { 0.0f, 1.0f, 0.0f };
        XMFLOAT4X4 worldView;
        flythrough_camera_update(
            &g_Scene.CameraPos.x,
            &g_Scene.CameraLook.x,
            up,
            &worldView.m[0][0],
            deltaTicks / (float)ticksPerSecond,
            100.0f * (GetAsyncKeyState(VK_LSHIFT) ? 2.0f : 1.0f) * activated,
            0.5f * activated,
            80.0f,
            currMouseX - g_Scene.LastMouseX, currMouseY - g_Scene.LastMouseY,
            GetAsyncKeyState('W'), GetAsyncKeyState('A'), GetAsyncKeyState('S'), GetAsyncKeyState('D'),
            GetAsyncKeyState(VK_SPACE), GetAsyncKeyState(VK_LCONTROL),
            FLYTHROUGH_CAMERA_LEFT_HANDED_BIT);

        D3D11_MAPPED_SUBRESOURCE mappedCamera;
        CHECKHR(dc->Map(g_Scene.pCameraBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCamera));

        float aspectWbyH = g_Scene.SceneViewport.Width / g_Scene.SceneViewport.Height;
        XMMATRIX viewProjection = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), aspectWbyH, 1.0f, 5000.0f);
        XMMATRIX worldViewProjection = XMMatrixMultiply(XMLoadFloat4x4(&worldView), viewProjection);

        PerCameraData* camera = (PerCameraData*)mappedCamera.pData;
        XMStoreFloat4x4(&camera->WorldViewProjection, XMMatrixTranspose(worldViewProjection));
        dc->Unmap(g_Scene.pCameraBuffer.Get(), 0);
    }

    const float kClearColor[] = {
        std::pow(100.0f / 255.0f, 2.2f),
        std::pow(149.0f / 255.0f, 2.2f),
        std::pow(237.0f / 255.0f, 2.2f),
        1.0f
    };
    dc->ClearRenderTargetView(pBackBufferRTV, kClearColor);
    dc->ClearDepthStencilView(g_Scene.pSceneDepthDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    ID3D11RenderTargetView* rtvs[] = { pBackBufferRTV };
    ID3D11DepthStencilView* dsv = g_Scene.pSceneDepthDSV.Get();
    dc->OMSetRenderTargets(_countof(rtvs), rtvs, dsv);
    
    dc->VSSetShader(g_Scene.SceneVS->VS, NULL, 0);
    dc->PSSetShader(g_Scene.ScenePS->PS, NULL, 0);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc->IASetInputLayout(g_Scene.pSceneInputLayout.Get());
    dc->RSSetState(g_Scene.pSceneRasterizerState.Get());
    dc->OMSetDepthStencilState(g_Scene.pSceneDepthStencilState.Get(), 0);
    dc->OMSetBlendState(g_Scene.pSceneBlendState.Get(), NULL, UINT_MAX);
    dc->RSSetViewports(1, &g_Scene.SceneViewport);
    
    ID3D11Buffer* sceneCBV = g_Scene.pCameraBuffer.Get();
    dc->VSSetConstantBuffers(CAMERA_BUFFER_SLOT, 1, &sceneCBV);

    int currMaterialID = -1;
    for (int sceneNodeID = 0; sceneNodeID < (int)g_Scene.SceneNodes.size(); sceneNodeID++)
    {
        SceneNode& sceneNode = g_Scene.SceneNodes[sceneNodeID];

        // Update Material CBV
        if (currMaterialID != sceneNode.MaterialID)
        {
            const Material& material = g_Scene.Materials[sceneNode.MaterialID];

            D3D11_MAPPED_SUBRESOURCE mapped;
            dc->Map(g_Scene.pMaterialBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

            PerMaterialData* materialData = (PerMaterialData*)mapped.pData;
            XMStoreFloat4(&materialData->Ambient, XMLoadFloat3(&material.Ambient));
            XMStoreFloat4(&materialData->Diffuse, XMLoadFloat3(&material.Diffuse));
            XMStoreFloat4(&materialData->Specular, XMLoadFloat3(&material.Specular));
            XMStoreFloat4(&materialData->Shininess, XMVectorReplicate(material.Shininess));
            XMStoreFloat4(&materialData->Opacity, XMVectorReplicate(material.Opacity));

            dc->Unmap(g_Scene.pSceneNodeBuffer.Get(), 0);
            
            ID3D11Buffer* materialCBV = g_Scene.pMaterialBuffer.Get();
            dc->PSSetConstantBuffers(MATERIAL_BUFFER_SLOT, 1, &materialCBV);

            currMaterialID = sceneNode.MaterialID;
        }
        
        // Update SceneNode CBV
        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            dc->Map(g_Scene.pSceneNodeBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

            PerSceneNodeData* sceneNodeData = (PerSceneNodeData*)mapped.pData;
            XMMATRIX worldMatrix = XMMatrixIdentity();
            worldMatrix = XMMatrixMultiply(worldMatrix, XMMatrixScalingFromVector(sceneNode.Transform.Scale));
            worldMatrix = XMMatrixMultiply(worldMatrix, XMMatrixRotationQuaternion(sceneNode.Transform.Quaternion));
            worldMatrix = XMMatrixMultiply(worldMatrix, XMMatrixTranslationFromVector(sceneNode.Transform.Translation));
            XMStoreFloat4x4(&sceneNodeData->WorldTransform, XMMatrixTranspose(worldMatrix));

            dc->Unmap(g_Scene.pSceneNodeBuffer.Get(), 0);

            ID3D11Buffer* sceneNodeCBV = g_Scene.pSceneNodeBuffer.Get();
            dc->VSSetConstantBuffers(SCENENODE_BUFFER_SLOT, 1, &sceneNodeCBV);
        }

        if (sceneNode.Type == SCENENODETYPE_STATICMESH)
        {
            StaticMesh& staticMesh = g_Scene.StaticMeshes[sceneNode.AsStaticMesh.StaticMeshID];
            
            ID3D11Buffer* staticMeshVertexBuffers[] = { 
                staticMesh.pPositionVertexBuffer.Get(), 
                staticMesh.pTexCoordVertexBuffer.Get(), 
                staticMesh.pNormalVertexBuffer.Get() 
            };
            UINT staticMeshStrides[] = { sizeof(VertexPosition), sizeof(VertexTexCoord), sizeof(VertexNormal) };
            UINT staticMeshOffsets[] = { 0, 0, 0 };
            dc->IASetVertexBuffers(0, _countof(staticMeshVertexBuffers), staticMeshVertexBuffers, staticMeshStrides, staticMeshOffsets);
            dc->IASetIndexBuffer(staticMesh.pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

            dc->DrawIndexed(staticMesh.IndexCountPerInstance, staticMesh.StartIndexLocation, 0);
        }
    }
    
    dc->OMSetRenderTargets(0, NULL, NULL);

    g_Scene.LastTicks = currTicks;
    g_Scene.LastMouseX = currMouseX;
    g_Scene.LastMouseY = currMouseY;
}