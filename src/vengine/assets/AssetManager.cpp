﻿#include <vengine/assets/AssetManager.hpp>
#include "vengine/Engine.hpp"
#include "vengine/drawing/Mesh.hpp"
#include "vengine/drawing/Texture.hpp"
#include <vengine/io/io.hpp>
#include <fstream>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <pugixml.hpp>

namespace vengine::assets {

bool AssetManager::SaveAsset(const std::filesystem::path &path,
                              const Ref<Asset> &asset) {
  OutFileBuffer outFile(path);
  if (!outFile.isOpen())
    return false;

  auto header = asset.Get()->GetHeader();
  outFile << header;

  MemoryBuffer assetData;
  asset->WriteTo(assetData);
  const uint64_t dataSize = assetData.size();
  outFile << dataSize;
  outFile << assetData;

  outFile << assetData;

  outFile.close();

  return true;
}

Ref<Asset> AssetManager::LoadAsset(
    const std::filesystem::path &path,
    const String &type, const std::function<Ref<Asset>()> &factory) {
  InFileBuffer inFile(path);
  if (!inFile.isOpen())
    return {};

  VEngineAssetHeader header;
  inFile >> header;

  if (header.type != type)
    return {};

  uint64_t dataSize;

  inFile >> dataSize;

  MemoryBuffer assetData;

  inFile >> assetData;

  inFile.close();

  auto asset = factory();
  if(asset) {
    return {};
  }

  asset->ReadFrom(assetData);

  return  asset;
}

Ref<drawing::Mesh> AssetManager::ImportMesh(
    const std::filesystem::path &path) {
  if (!std::filesystem::exists(path)) {
    return {};
  }

  fastgltf::GltfDataBuffer data;
  data.loadFromFile(path);

  constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers |
                               fastgltf::Options::LoadGLBBuffers;

  fastgltf::Asset gltf;
  fastgltf::Parser parser;
  if (auto load = parser.
      loadGltfBinary(&data, path.parent_path(), gltfOptions)) {
    gltf = std::move(load.get());
  } else {
    GetLogger()->error("Failed to load glTF: {} \n",
                       fastgltf::to_underlying(load.error()));
    return {};
  }

  // Only Import First Mesh
  if (gltf.meshes.empty()) {
    return {};
  }

  auto [primitives, weights, name] = gltf.meshes[0];

  Array<drawing::MeshSurface> surfaces;
  Array<uint32_t> indices;
  Array<drawing::Vertex> vertices;
  VEngineAssetHeader header;
  header.name = String(name);
  header.type = types::MESH;

  for (auto &&primitive : primitives) {
    drawing::MeshSurface newSurface{};
    newSurface.startIndex = static_cast<uint32_t>(indices.size());
    newSurface.count = gltf.accessors[primitive.indicesAccessor.value()].count;

    auto initialVtx = vertices.size();
    // Load indexes
    {
      fastgltf::Accessor &indexAccessor = gltf.accessors[primitive.
        indicesAccessor.value()];
      indices.reserve(indices.size() + indexAccessor.count);
      fastgltf::iterateAccessor<uint32_t>(gltf, indexAccessor,
                                          [&](const uint32_t idx) {
                                            auto index = static_cast<uint32_t>(
                                              idx + initialVtx);
                                            indices.Push(index);
                                          });
    }

    // Load Vertex Locations
    {
      fastgltf::Accessor &locationAccessor = gltf.accessors[primitive.
        findAttribute("POSITION")->second];
      vertices.resize(vertices.size() + locationAccessor.count);

      fastgltf::iterateAccessorWithIndex<glm::vec3>(
          gltf, locationAccessor, [&](glm::vec3 v, const size_t idx) {
            const drawing::Vertex newVertex{{v.x, v.y, v.z, 0}, {1, 0, 0, 0},
                                            {0, 0, 0, 0}};
            vertices[initialVtx + idx] = newVertex;
          });
    }

    // Load Vertex Normals
    if (auto normals = primitive.findAttribute("NORMAL");
      normals != primitive.attributes.end()) {
      fastgltf::iterateAccessorWithIndex<glm::vec3>(
          gltf, gltf.accessors[(*normals).second],
          [&](glm::vec3 v, const size_t idx) {
            vertices[initialVtx + idx].normal = {v.x, v.y, v.z, 0};
          });
    }

    // Load Vertex UVs
    if (auto uv = primitive.findAttribute("TEXCOORD_0");
      uv != primitive.attributes.end()) {

      fastgltf::iterateAccessorWithIndex<glm::vec2>(
          gltf, gltf.accessors[(*uv).second],
          [&](glm::vec2 v, const size_t idx) {
            vertices[initialVtx + idx].uv = {v.x, v.y, 0, 0};
          });
    }
    surfaces.Push(newSurface);
  }

  auto result = newSharedObject<drawing::Mesh>();
  result->SetVertices(vertices);
  result->SetIndices(indices);
  result->SetSurfaces(surfaces);
  result->SetHeader(header);
  result->Init(GetOuter()->GetDrawer().Reserve().Get());

  GetLogger()->info("Imported Mesh {}",path.string());
  return result;
}

std::vector<Ref<drawing::Mesh>> AssetManager::ImportMeshes(
    const std::vector<std::filesystem::path> &paths) {
  std::vector<Ref<drawing::Mesh>> results;
  
  for(auto &path : paths) {
    if(auto result = ImportMesh(path)) {
      results.emplace_back(result);
    }
  }

  return results;
}

Ref<drawing::Mesh> AssetManager::LoadMeshAsset(
    const std::filesystem::path &path) {
  
  const auto asset = LoadAsset(path,types::MESH,[] {
    return newSharedObject<drawing::Mesh>();
  });

  if(!asset) {
    return {};
  }

  auto mesh = asset.Cast<drawing::Mesh>();

  if(!mesh) {
    return {};
  }

  mesh->Init(GetOuter()->GetDrawer().Reserve().Get());

  return mesh;
}

Ref<drawing::Texture> AssetManager::ImportTexture(
    const std::filesystem::path &path) {

  auto img = cv::imread(path.string(),cv::IMREAD_UNCHANGED);
  if(img.channels() == 3) {
    cv::cvtColor(img.clone(),img,cv::COLOR_BGR2RGBA);
  } else {
    cv::cvtColor(img.clone(),img,cv::COLOR_BGRA2RGBA);
  }

  const vk::Extent3D imageSize{static_cast<uint32_t>(img.cols) ,static_cast<uint32_t>(img.rows),1};
  Array<unsigned char> vecData;
  vecData.insert(vecData.end(),img.data,img.data + (imageSize.width * imageSize.height * img.channels()));
  auto tex = drawing::Texture::FromData(GetOuter()->GetDrawer().Reserve().Get(),vecData,imageSize,vk::Format::eR8G8B8A8Unorm,vk::Filter::eLinear);
  VEngineAssetHeader header{};
  header.type = types::TEXTURE;
  header.name = path.filename().string();
  tex->SetHeader(header);
  GetLogger()->info("Imported Texture {}",path.string());
  return tex;
}

std::vector<Ref<drawing::Texture>> AssetManager::ImportTextures(
    const std::vector<std::filesystem::path> &paths) {
  std::vector<Ref<drawing::Texture>> results;
  
  for(auto &path : paths) {
    if(auto result = ImportTexture(path)) {
      results.emplace_back(result);
    }
  }

  return results;
}

Ref<drawing::Texture> AssetManager::LoadTextureAsset(
    const std::filesystem::path &path) {
  const auto asset = LoadAsset(path,types::TEXTURE,[] {
    return newSharedObject<drawing::Texture>();
  });

  if(!asset) {
    return {};
  }

  auto texture = asset.Cast<drawing::Texture>();

  if(!texture) {
    return {};
  }

  texture->Init(GetOuter()->GetDrawer().Reserve().Get());

  return texture;
}

Ref<widget::Font> AssetManager::ImportFont(
    const std::filesystem::path &path) {
  if (!std::filesystem::exists(path)) {
    return {};
  }

  const auto folderName = path.filename().string();

  const auto fntFile = path / (folderName + ".fnt");

  pugi::xml_document xmlFile;
  const pugi::xml_parse_result result = xmlFile.load_file(fntFile.string().c_str());
  
  if(!result) {
    GetLogger()->error("Failed to parse font xml {}",fntFile.string().c_str());
    return {};
  }

  Array<Ref<drawing::Texture>> textures;
  auto clearTextures = [&textures] {
    textures.clear();
  };
  
  for(auto page : xmlFile.child("font").child("pages").children()) {
    auto pageFileName = page.attribute("file").as_string();
    auto pageFilePath = path / pageFileName;
    auto texture = ImportTexture(pageFilePath);

    
    if(!texture) {
      clearTextures();
      GetLogger()->error("Failed to import font texture",pageFilePath.string().c_str());
      return {};
    }

    texture->SetFilter(vk::Filter::eNearest);
    texture->SetMipMapped(true);
    
    texture->Init(GetEngine()->GetDrawer().Reserve().Get());
    textures.Push(texture);
  }

  
  std::unordered_map<uint32_t,widget::FontCharacter> chars;
  std::unordered_map<uint32_t,uint32_t> charsIndices;
  uint32_t curIdx = 0;
  for(auto xmlChar : xmlFile.child("font").child("chars").children()) {
    const auto atlasId = xmlChar.attribute("page").as_int();
    const auto atlas = textures[atlasId];
    const auto atlasSize = atlas->GetSize();
    auto charId = xmlChar.attribute("id").as_uint();
    widget::FontCharacter fChar{};
    fChar.atlasWidth = atlasSize.width;
    fChar.atlasHeight = atlasSize.height;
    fChar.x = xmlChar.attribute("x").as_int();
    fChar.y = xmlChar.attribute("y").as_int();
    fChar.width = xmlChar.attribute("width").as_int();
    fChar.height = xmlChar.attribute("height").as_int();
    fChar.xOffset = xmlChar.attribute("xoffset").as_int();
    fChar.yOffset = xmlChar.attribute("yoffset").as_int();
    fChar.xAdvance = xmlChar.attribute("xadvance").as_int();
    fChar.atlasId = atlasId;
    
    chars.insert({charId,fChar});
    charsIndices.insert({charId,curIdx});
    
    curIdx++;
  }

  auto font = newSharedObject<widget::Font>();
  VEngineAssetHeader fontHeader{};
  fontHeader.type = types::FONT;
  fontHeader.name = String(xmlFile.child("font").child("info").attribute("face").as_string());
  font->SetHeader(fontHeader);
  font->SetChars(chars,charsIndices);
  font->SetTextures(textures);
  font->Init(GetEngine()->GetWidgetManager().Reserve().Get());
  return font;
}

Ref<widget::Font> AssetManager::LoadFontAsset(
    const std::filesystem::path &path) {
  const auto asset = LoadAsset(path,types::FONT,[] {
    return newSharedObject<widget::Font>();
  });

  if(!asset) {
    return {};
  }

  auto font = asset.Cast<widget::Font>();

  if(!font) {
    return {};
  }

  font->Init(GetEngine()->GetWidgetManager().Reserve().Get());

  return font;
}

String AssetManager::GetName() const {
  return "assets";
}

}
