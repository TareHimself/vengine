﻿#include <aerox/drawing/ShaderManager.hpp>
#include <aerox/drawing/Shader.hpp>
#include "aerox/utils.hpp"
#include "aerox/io/io.hpp"
//#include <glslang/MachineIndependent/localintermediate.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <aerox/drawing/DrawingSubsystem.hpp>

namespace aerox::drawing {

GlslShaderIncluder::GlslShaderIncluder(ShaderManager * manager,const fs::path &inPath) {
  _manager = manager;
  sourceFilePath = inPath;
  if(_bDebug){
    _manager->GetLogger()->Info("Shader Includer Created");
  }
}

glslang::TShader::Includer::IncludeResult * GlslShaderIncluder::includeSystem(const char*filePath, const char *includerName, size_t inclusionDepth) {
  const fs::path actualPath(filePath);
  if(_bDebug) {
    _manager->GetLogger()->Info("Including Shader File {:s}",actualPath.string());
  }
  const auto fileContent = new std::string(io::readFileAsString(actualPath));
  auto result = new IncludeResult(actualPath.string(),fileContent->c_str(),fileContent->size(),fileContent);
  _results.Add(result);
  return result;
}

glslang::TShader::Includer::IncludeResult * GlslShaderIncluder::includeLocal(const char*filePath, const char *includerName, size_t inclusionDepth) {
  
  const auto actualPath = sourceFilePath.parent_path() / fs::path(filePath);
  if(_bDebug) {
   _manager->GetLogger()->Info("Including Shader File {:s}",actualPath.string());
  }
  const auto fileContent = new std::string(io::readFileAsString(actualPath));
  auto result = new IncludeResult(actualPath.string(),fileContent->c_str(),fileContent->size(),fileContent);
  _results.Add(result);
  return result;
}

void GlslShaderIncluder::releaseInclude(IncludeResult *result) {
  if(result != nullptr) {
    _results.Remove(result);
    delete static_cast<std::string *>(result->userData);
    delete result;
  }
}

GlslShaderIncluder::~GlslShaderIncluder() {

  for(const auto result : _results) {
    delete static_cast<std::string *>(result->userData);
    delete result;
  }
  
  _results.clear();
}


EShLanguage ShaderManager::GetLang(const fs::path &shaderPath) {
  const auto ext = shaderPath.extension().string().substr(1);

  if (ext == "vert") {
    return EShLangVertex;
  }

  if (ext == "tesc") {
    return EShLangTessControl;
  }

  if (ext == "tese") {
    return EShLangTessEvaluation;
  }

  if (ext == "geom") {
    return EShLangGeometry;
  }
  if (ext == "frag") {
    return EShLangFragment;
  }
  if (ext == "comp") {
    return EShLangCompute;
  }

  if (ext == "rgen") {
    return EShLangRayGen;
  }
  if (ext == "rint") {
    return EShLangIntersect;
  }
  if (ext == "rahit") {
    return EShLangAnyHit;
  }
  if (ext == "rchit") {
    return EShLangClosestHit;
  }
  if (ext == "rmiss") {
    return EShLangMiss;
  }
  if (ext == "rcall") {
    return EShLangCallable;
  }
  if (ext == "mesh") {
    return EShLangMesh;
  }
  if (ext == "task") {
    return EShLangTask;
  }

  return EShLangVertex;
}

bool ShaderManager::HasLoadedShader(
    const fs::path &shaderPath) const {
  return _shaders.contains(shaderPath);
}

std::shared_ptr<Shader> ShaderManager::GetLoadedShader(
    const fs::path &shaderPath) const {
  if(!HasLoadedShader(shaderPath)){
    return {};
  }

  return _shaders.at(shaderPath);
}

Array<unsigned int> ShaderManager::Compile(const fs::path &shaderPath){
  if (!fs::exists(shaderPath)) {
    throw std::runtime_error(
        std::string("Shader file does not exist: {}") + shaderPath.string());
  }

  GetLogger()->Info("Compiling Shader from file: {}",shaderPath.string());
  
  const auto lang = GetLang(shaderPath);
  
  const auto shader = new glslang::TShader(lang);

  const auto shaderFileContent = io::readFileAsString(shaderPath);

  shader->setEnvClient(glslang::EShClient::EShClientVulkan,glslang::EShTargetVulkan_1_3);
  shader->setEnvTarget(glslang::EshTargetSpv,glslang::EShTargetSpv_1_3);
  const char *sourcePtr = shaderFileContent.c_str();
  const char *const *sourcePtrArr = &(sourcePtr);
  const int sourcePtrSize = shaderFileContent.size();
  
  shader->setStringsWithLengths(sourcePtrArr,&sourcePtrSize,1);
  shader->setSourceEntryPoint("main");
  shader->setEntryPoint("main");
  

  //shader->getIntermediate()->setSource(glslang::EShSourceGlsl);

  constexpr auto message = static_cast<EShMessages>(EShMessages::EShMsgVulkanRules | EShMessages::EShMsgSpvRules | EShMsgDebugInfo);

  GlslShaderIncluder includer(this,shaderPath);
  const auto resources =  GetResources();
  resources->maxDrawBuffers = true;
  resources->maxCombinedImageUnitsAndFragmentOutputs = 128;
  resources->maxComputeWorkGroupSizeX = 128;
  resources->maxComputeWorkGroupSizeY = 128;
  resources->maxComputeWorkGroupSizeZ = 128;
  resources->maxDrawBuffers = 10;
  resources->limits.nonInductiveForLoops = true;
  resources->limits.whileLoops = true;
  resources->limits.doWhileLoops = true;
  resources->limits.generalUniformIndexing = true;
  resources->limits.generalAttributeMatrixVectorIndexing = true;
  resources->limits.generalVaryingIndexing = true;
  resources->limits.generalSamplerIndexing = true;
  resources->limits.generalVariableIndexing = true;
  resources->limits.generalConstantMatrixVectorIndexing = true;

  const auto result = shader->parse(resources,450,ENoProfile,false,false,message,includer);
  auto err = std::string(shader->getInfoLog());
  utils::vassert(result,"Failed to parse shader [{}] \n {}",shaderPath.string().c_str(),err);


  Array<unsigned int> spvResult;
  
  {
    glslang::TProgram program;
    program.addShader(shader);

    if(!program.link(message)) {
      throw std::runtime_error(std::string("Failed to parse shader at path: ") + shaderPath.string() + "\n" + program.getInfoLog());
    }

    glslang::SpvOptions options{};

    // Needed for reflection
    options.generateDebugInfo=true;
    options.stripDebugInfo=false;
    options.disableOptimizer=false;
    options.emitNonSemanticShaderDebugInfo = true;
    
    
    glslang::GlslangToSpv(*program.getIntermediate(lang),spvResult,&options);
    //program.buildReflection(EShReflectionOptions::EShReflectionAllIOVariables);
  }

  delete shader;

  GetLogger()->Info("Compiled Shader from file: {}",shaderPath.string());
  return spvResult;
}

Array<unsigned> ShaderManager::CompileAndSave(
    const fs::path &shaderPath){
  const auto compiledDir = shaderPath.parent_path() / "compiled";
  const auto compiledPath = compiledDir / (shaderPath.filename().string() + ".spv");
  const auto compiledHashPath = compiledDir / (shaderPath.filename().string() + ".hash");
  
  auto compiledData = Compile(shaderPath);
    
  if(!fs::exists(compiledDir)) {
    fs::create_directory(compiledDir);
  }

  glslang::OutputSpvBin(compiledData,compiledPath.string().c_str());
  
  const auto sourceFile = io::readFileAsString(shaderPath);
  
  const auto sourceFileHash = utils::hash(sourceFile.data(),sourceFile.size() * sizeof(char));
  
  io::writeStringToFile(compiledHashPath,sourceFileHash);
  
  return compiledData;
}

Array<unsigned int> ShaderManager::LoadOrCompileSpv(const fs::path &shaderPath){
  GetLogger()->Info("Loading Shader {}",shaderPath.string());
  const auto compiledDir = shaderPath.parent_path() / "compiled";
  const auto compiledPath = compiledDir / (shaderPath.filename().string() + ".spv");
  const auto compiledHashPath = compiledDir / (shaderPath.filename().string() + ".hash");

  if(fs::exists(compiledDir) && fs::exists(compiledPath) && fs::exists(compiledHashPath)) {
    auto shader = io::readFile<unsigned int>(compiledPath);
    const auto oldShaderHash = io::readFileAsString(compiledHashPath);
    const auto sourceFile = io::readFileAsString(shaderPath);

    if(const auto newShaderHash = utils::hash(sourceFile.data(),sourceFile.size() * sizeof(char)); oldShaderHash == newShaderHash) {
      GetLogger()->Info("Loaded shader from disk: {}",shaderPath.string());
      return shader;
    }
    
    GetLogger()->Info("Detected change in source file: {}",shaderPath.string());
    return CompileAndSave(shaderPath);
  }
  
  return CompileAndSave(shaderPath);
}

std::shared_ptr<Shader> ShaderManager::RegisterShader(std::shared_ptr<Shader> shader) {
  _shaders.insert({shader->GetSourcePath(),shader});
  
  shader->Init(this);
  
  return shader;
}

std::shared_ptr<Shader> ShaderManager::CreateShader(
    const fs::path &path) {
  return Shader::FromSource(path);
}

void ShaderManager::UnRegisterShader(const Shader *shader) {
  if(!IsPendingDestroy()) {
    if(_shaders.contains(shader->GetSourcePath())) {
      _shaders.erase(_shaders.find(shader->GetSourcePath()));
    }
  }
}

void ShaderManager::OnInit(DrawingSubsystem * owner) {
  TOwnedBy::OnInit(owner);
  glslang::InitializeProcess();
  InitLogger("shaders");
}


void ShaderManager::OnDestroy() {
  Object::OnDestroy();
  
  _shaders.clear();
  
  glslang::FinalizeProcess();
}

}
