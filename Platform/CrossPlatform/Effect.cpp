#define NOMINMAX
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Base/DefaultFileLoader.h"
#include "Simul/Base/StringFunctions.h"
#include <iostream>
#include <algorithm>

using namespace simul;
using namespace crossplatform;
using namespace std;

bool EffectPass::usesTextureSlot(int s) const
{
	if(s>=1000)
		return usesRwTextureSlot(s-1000);
	unsigned m=((unsigned)1<<(unsigned)s);
	return (textureSlots&m)!=0;
}

bool EffectPass::usesTextureSlotForSB(int s) const
{
	if(s>=1000)
		return usesRwTextureSlotForSB(s-1000);
	unsigned m=((unsigned)1<<(unsigned)s);
	return (textureSlotsForSB&m)!=0;
}

bool EffectPass::usesRwTextureSlotForSB(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (rwTextureSlotsForSB&m)!=0;
}

bool EffectPass::usesBufferSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (bufferSlots&m)!=0;
}

bool EffectPass::usesSamplerSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return true;//(samplerSlots&m)!=0;
}

bool EffectPass::usesRwTextureSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (rwTextureSlots&m)!=0;
}


bool EffectPass::usesTextures() const
{
	return (textureSlots+rwTextureSlots)!=0;
}

bool EffectPass::usesSBs() const
{
	return (textureSlotsForSB+rwTextureSlotsForSB)!=0;
}

bool EffectPass::usesBuffers() const
{
	return bufferSlots!=0;
}

bool EffectPass::usesSamplers() const
{
	return samplerSlots!=0;
}


void EffectPass::SetUsesBufferSlots(unsigned s)
{
	bufferSlots|=s;
}

void EffectPass::SetUsesTextureSlots(unsigned s)
{
	textureSlots|=s;
}

void EffectPass::SetUsesTextureSlotsForSB(unsigned s)
{
	textureSlotsForSB|=s;
}

void EffectPass::SetUsesRwTextureSlots(unsigned s)
{
	rwTextureSlots|=s;
}

void EffectPass::SetUsesRwTextureSlotsForSB(unsigned s)
{
	rwTextureSlotsForSB|=s;
}

void EffectPass::SetUsesSamplerSlots(unsigned s)
{
	samplerSlots|=s;
}

EffectTechniqueGroup::~EffectTechniqueGroup()
{
	for (crossplatform::TechniqueMap::iterator i = techniques.begin(); i != techniques.end(); i++)
	{
		delete i->second;
	}
	techniques.clear();
	charMap.clear();
}

Effect::Effect()
	:renderPlatform(NULL)
	,apply_count(0)
	,currentPass(0)
	,currentTechnique(NULL)
	,platform_effect(NULL)
{
}

Effect::~Effect()
{
	InvalidateDeviceObjects();
	SIMUL_ASSERT(apply_count==0);
}

void Effect::InvalidateDeviceObjects()
{
	shaderResources.clear();
	for (auto i = groups.begin(); i != groups.end(); i++)
	{
		delete i->second;
	}
	groups.clear();
	groupCharMap.clear();
}

EffectTechnique::EffectTechnique()
	:platform_technique(NULL)
	,should_fence_outputs(true)
{
}

EffectTechnique::~EffectTechnique()
{
}

EffectTechnique *EffectTechniqueGroup::GetTechniqueByName(const char *name)
{
	TechniqueCharMap::iterator i=charMap.find(name);
	if(i!=charMap.end())
		return i->second;
	TechniqueMap::iterator j=techniques.find(name);
	if(j==techniques.end())
		return NULL;
	charMap[name]=j->second;
	return j->second;
}

crossplatform::EffectTechnique *Effect::GetTechniqueByName(const char *name)
{
	return groupCharMap[0]->GetTechniqueByName(name);
}


EffectTechnique *EffectTechniqueGroup::GetTechniqueByIndex(int index)
{
	return techniques_by_index[index];
}

int EffectTechnique::NumPasses() const
{
	return (int)passes_by_name.size();
}

EffectPass *EffectTechnique::GetPass(int i)
{
	return passes_by_index[i];
}

EffectPass *EffectTechnique::GetPass(const char *name)
{
	return passes_by_name[name];
}


EffectTechniqueGroup *Effect::GetTechniqueGroupByName(const char *name)
{
	auto i=groupCharMap.find(name);
	if(i!=groupCharMap.end())
		return i->second;
	auto j=groups.find(name);
	if(j!=groups.end())
		return j->second;
	return nullptr;
}


void Effect::SetTexture(crossplatform::DeviceContext &deviceContext,crossplatform::ShaderResource &res,crossplatform::Texture *tex,int index,int mip)
{
	// If not valid, we've already put out an error message when we assigned the resource, so fail silently. Don't risk overwriting a slot.
	if(!res.valid)
		return;
	crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
	unsigned long slot=res.slot;
	unsigned long dim=res.dimensions;
#ifdef _DEBUG
	if(!tex)
	{
		return;
	}
	if(!tex->IsValid())
	{
		SIMUL_BREAK_ONCE("Invalid texture applied");
		return;
	}
#endif
	crossplatform::TextureAssignment &ta=cs->textureAssignmentMap[slot];
	ta.resourceType=res.shaderResourceType;
	ta.texture=(tex&&tex->IsValid()&&res.valid)?tex:0;
	ta.dimensions=dim;
	ta.uav=false;
	ta.index=index;
	ta.mip=mip;
	cs->textureAssignmentMapValid=false;
}

void Effect::SetTexture(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture *tex,int index,int mip)
{
	crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
	int slot = GetSlot(name);
	if(slot<0)
	{
		SIMUL_CERR<<"Didn't find Texture "<<name<<std::endl;
		return;
	}
	int dim = GetDimensions(name);
	crossplatform::TextureAssignment &ta=cs->textureAssignmentMap[slot];
	ta.resourceType=GetResourceType(name);
	ta.texture=(tex&&tex->IsValid())?tex:0;
	ta.dimensions=dim;
	ta.uav=false;
	ta.index=index;
	ta.mip=mip;
	cs->textureAssignmentMapValid=false;
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext, crossplatform::ShaderResource &res, crossplatform::Texture *tex,int index,int mip)
{
	// If not valid, we've already put out an error message when we assigned the resource, so fail silently. Don't risk overwriting a slot.
	if(!res.valid)
		return;
	crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
	unsigned long slot=res.slot+1000;//1000+((combined&(0xFFFF0000))>>16);
	unsigned long dim=res.dimensions;//combined&0xFFFF;
	auto &ta=cs->textureAssignmentMap[slot];
	ta.resourceType=res.shaderResourceType;
	ta.texture=(tex&&tex->IsValid()&&res.valid)?tex:0;
	ta.dimensions=dim;
	ta.uav=true;
	ta.mip=mip;
	ta.index=index;
	cs->textureAssignmentMapValid=false;
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext, const char *name, crossplatform::Texture *t,int index, int mip)
{
	const ShaderResource *i=GetTextureDetails(name);
	if(!i)
	{
		SIMUL_CERR<<"Didn't find UAV "<<name<<std::endl;
		return;
	}
	crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
	// Make sure no slot clash between uav's and srv's:
	int slot = 1000+GetSlot(name);
	{
		int dim = GetDimensions(name);
		crossplatform::TextureAssignment &ta=cs->textureAssignmentMap[slot];
		ta.resourceType=GetResourceType(name);
		ta.texture=t;
		ta.dimensions=dim;

		ta.uav=true;
		ta.mip=mip;
		ta.index=index;
	}
	cs->textureAssignmentMapValid=false;
}

crossplatform::ShaderResource Effect::GetShaderResource(const char *name)
{
	crossplatform::ShaderResource res;
	auto i = GetTextureDetails(name);
	if (!i)
	{
		res.valid = false;
		SIMUL_CERR << "Invalid Shader resource name: " << (name ? name : "") << std::endl;
		SIMUL_BREAK_ONCE("")
			return res;
	}
	else
		res.valid = true;
	unsigned slot = GetSlot(name);
	unsigned dim = GetDimensions(name);
	res.platform_shader_resource = (void*)nullptr;
	res.slot = slot;
	res.shaderResourceType = GetResourceType(name);
	return res;
}

EffectDefineOptions simul::crossplatform::CreateDefineOptions(const char *name,const char *option1)
{
	EffectDefineOptions o;
	o.name=name;
	o.options.push_back(std::string(option1));
	return o;
}

EffectDefineOptions simul::crossplatform::CreateDefineOptions(const char *name,const char *option1,const char *option2)
{
	EffectDefineOptions o;
	o.name=name;
	o.options.push_back(std::string(option1));
	o.options.push_back(std::string(option2));
	return o;
}
EffectDefineOptions simul::crossplatform::CreateDefineOptions(const char *name,const char *option1,const char *option2,const char *option3)
{
	EffectDefineOptions o;
	o.name=name;
	o.options.push_back(std::string(option1));
	o.options.push_back(std::string(option2));
	o.options.push_back(std::string(option3));
	return o;
}

EffectTechnique *Effect::EnsureTechniqueExists(const string &groupname,const string &techname_,const string &passname)
{
	EffectTechnique *tech=NULL;
	string techname=techname_;
	{
		if(groups.find(groupname)==groups.end())
		{
			groups[groupname]=new crossplatform::EffectTechniqueGroup;
			if(groupname.length()==0)
				groupCharMap[nullptr]=groups[groupname];
		}
		crossplatform::EffectTechniqueGroup *group=groups[groupname];
		if(group->techniques.find(techname)!=group->techniques.end())
			tech=group->techniques[techname];
		else
		{
			tech								=CreateTechnique(); 
			int index							=(int)group->techniques.size();
			group->techniques[techname]			=tech;
			group->techniques_by_index[index]	=tech;
			if(groupname.size())
				techname=(groupname+"::")+techname;
			techniques[techname]			=tech;
			techniques_by_index[index]		=tech;
			tech->name						=techname;
		}
	}
	return tech;
}

const char *Effect::GetTechniqueName(const EffectTechnique *t) const
{
	for(auto g=groups.begin();g!=groups.end();g++)
	for(crossplatform::TechniqueMap::const_iterator i=g->second->techniques.begin();i!=g->second->techniques.end();i++)
	{
		if(i->second==t)
			return i->first.c_str();
	}
	return "";
}

void Effect::Apply(DeviceContext &deviceContext,const char *tech_name,const char *pass)
{
	Apply(deviceContext,GetTechniqueByName(tech_name),pass);
}

void Effect::Apply(DeviceContext &deviceContext,const char *tech_name,int pass)
{
	Apply(deviceContext,GetTechniqueByName(tech_name),pass);
}


void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass_num)
{
	if (apply_count != 0)
		SIMUL_BREAK("Effect::Apply without a corresponding Unapply!")
	apply_count++;
	currentTechnique				=effectTechnique;
	if(effectTechnique)
	{
		EffectPass *p				=(effectTechnique)->GetPass(pass_num>=0?pass_num:0);
		if(p)
		{
			crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
			cs->invalidate();
			cs->currentEffectPass=p;
			cs->currentTechnique=effectTechnique;
			cs->currentEffect=this;
		}
		else
			SIMUL_BREAK("No pass found");
	}
	currentPass=pass_num;
}


void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *passname)
{
	currentTechnique				=effectTechnique;
	if (apply_count != 0)
		SIMUL_BREAK("Effect::Apply without a corresponding Unapply!")
	apply_count++;
	currentTechnique = effectTechnique;
	if (effectTechnique)
	{
		EffectPass *p = NULL;
		if(passname)
			p=effectTechnique->GetPass(passname);
		else
			p=effectTechnique->GetPass(0);
		if(p)
		{
			crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
			cs->invalidate();
			cs->currentTechnique=effectTechnique;
			cs->currentEffectPass=p;
			cs->currentEffect=this;
		}
		else
			SIMUL_BREAK("No pass found");
	}
}

void Effect::Unapply(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::ContextState *cs = renderPlatform->GetContextState(deviceContext);
	cs->currentEffectPass = NULL;
	cs->currentEffect = NULL;
	if (apply_count <= 0)
		SIMUL_BREAK("Effect::Unapply without a corresponding Apply!")
	else if (apply_count>1)
		SIMUL_BREAK("Effect::Apply has been called too many times!")
		apply_count--;
	currentTechnique = NULL;
}
void Effect::StoreConstantBufferLink(crossplatform::ConstantBufferBase *b)
{
	linkedConstantBuffers.insert(b);
}

bool Effect::IsLinkedToConstantBuffer(crossplatform::ConstantBufferBase*b) const
{
	return (linkedConstantBuffers.find(b)!=linkedConstantBuffers.end());
}


const ShaderResource *Effect::GetTextureDetails(const char *name)
{
	auto j=textureCharMap.find(name);
	if(j!=textureCharMap.end())
		return j->second;
	for(auto i:textureDetailsMap)
	{
		if(strcmp(i.first.c_str(),name)==0)
		{
			textureCharMap[name]=i.second;
			return i.second;
		}
	}
	return nullptr;
}

int Effect::GetSlot(const char *name)
{
	const ShaderResource *i=GetTextureDetails(name);
	if(!i)
		return -1;
	return i->slot;
}

int Effect::GetSamplerStateSlot(const char *name)
{
	auto i=samplerStates.find(std::string(name));
	if(i==samplerStates.end())
		return -1;
	return i->second->default_slot;
}

int Effect::GetDimensions(const char *name)
{
	const ShaderResource *i=GetTextureDetails(name);
	if(!i)
		return -1;
	return i->dimensions;
}

crossplatform::ShaderResourceType Effect::GetResourceType(const char *name)
{
	const ShaderResource *i=GetTextureDetails(name);
	if(!i)
		return crossplatform::ShaderResourceType::COUNT;
	return i->shaderResourceType;
}

void Effect::UnbindTextures(crossplatform::DeviceContext &deviceContext)
{
	if(!renderPlatform)
		return;
	crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
	cs->textureAssignmentMap.clear();
	cs->samplerStateOverrides.clear();
	cs->applyBuffers.clear();  //Because we might otherwise have invalid data
	cs->applyVertexBuffers.clear();
	cs->invalidate();
}

static bool is_equal(std::string str,const char *tst)
{
	return (_stricmp(str.c_str(),tst) == 0);
}

crossplatform::SamplerStateDesc::Wrapping stringToWrapping(string s)
{
	if(is_equal(s,"WRAP"))
		return crossplatform::SamplerStateDesc::WRAP;
	if(is_equal(s,"CLAMP"))
		return crossplatform::SamplerStateDesc::CLAMP;
	if(is_equal(s,"MIRROR"))
		return crossplatform::SamplerStateDesc::MIRROR;
	SIMUL_BREAK((string("Invalid string")+s).c_str());
	return crossplatform::SamplerStateDesc::WRAP;
}

crossplatform::SamplerStateDesc::Filtering stringToFilter(string s)
{
	if(is_equal(s,"POINT"))
		return crossplatform::SamplerStateDesc::POINT;
	if(is_equal(s,"LINEAR"))
		return crossplatform::SamplerStateDesc::LINEAR;
	SIMUL_BREAK((string("Invalid string")+s).c_str());
	return crossplatform::SamplerStateDesc::POINT;
}

static int toInt(string s)
{
	const char *t=s.c_str();
	return atoi(t);
}

static crossplatform::CullFaceMode toCullFadeMode(string s)
{
	if(is_equal(s,"CULL_BACK"))
		return crossplatform::CULL_FACE_BACK;
	else if(is_equal(s,"CULL_FRONT"))
		return crossplatform::CULL_FACE_FRONT;
	else if(is_equal(s,"CULL_FRONTANDBACK"))
		return crossplatform::CULL_FACE_FRONTANDBACK;
	else if(is_equal(s,"CULL_NONE"))
		return crossplatform::CULL_FACE_NONE;
	SIMUL_BREAK((string("Invalid string")+s).c_str());
	crossplatform::CULL_FACE_NONE;
}

static crossplatform::PolygonMode toPolygonMode(string s)
{
	if(is_equal(s,"FILL_SOLID"))
		return crossplatform::PolygonMode::POLYGON_MODE_FILL;
	else if(is_equal(s,"FILL_WIREFRAME"))
		return crossplatform::PolygonMode::POLYGON_MODE_LINE;
	else if(is_equal(s,"FILL_POINT"))
		return crossplatform::PolygonMode::POLYGON_MODE_POINT;
}

static bool toBool(string s)
{
	string ss=s.substr(0,4);
	const char *t=ss.c_str();
	if(_stricmp(t,"true")==0)
		return true;
	ss=s.substr(0,5);
	t=ss.c_str();
	if(_stricmp(t,"false")==0)
		return false;
	SIMUL_CERR<<"Unknown bool "<<s<<std::endl;
	return false;
}

void Effect::Load(crossplatform::RenderPlatform *r, const char *filename_utf8, const std::map<std::string, std::string> &defines)
{
	renderPlatform=r;
	groups.clear();
	groupCharMap.clear();
	for(auto i:textureDetailsMap)
	{
		delete i.second;
	}
	textureDetailsMap.clear();
	textureCharMap.clear();
	// We will load the .sfxo file, which contains the list of shader binary files, and also the arrangement of textures, buffers etc. in numeric slots.

	std::string filenameUtf8=renderPlatform->GetShaderBinaryPath();
	if (filenameUtf8[filenameUtf8.length() - 1] != '/')
		filenameUtf8+="/";
	filenameUtf8+=filename_utf8;
	if (filenameUtf8.find(".") >= filenameUtf8.length())
		filenameUtf8 += ".sfxo";
	if(!simul::base::FileLoader::GetFileLoader()->FileExists(filenameUtf8.c_str()))
	{
		// Some engines force filenames to lower case because reasons:
		std::transform(filenameUtf8.begin(), filenameUtf8.end(), filenameUtf8.begin(), ::tolower);
		if(!simul::base::FileLoader::GetFileLoader()->FileExists(filenameUtf8.c_str()))
		{
			SIMUL_CERR<<"Shader file not found: "<<filenameUtf8.c_str()<<std::endl;
			filenameUtf8=filename_utf8;
		// The sfxo does not exist, so we can't load this effect.
			return;
		}
	}
	void *ptr;
	unsigned int num_bytes;
	simul::base::FileLoader::GetFileLoader()->AcquireFileContents(ptr,num_bytes,filenameUtf8.c_str(),true);
	const char *txt=(const char *)ptr;
	std::string str;
	str.reserve(num_bytes);
	str=txt;
	str.resize((size_t)num_bytes, 0);
	// Load all the .sb's
	int pos					=0;
	int next				=(int)str.find('\n',pos+1);
	int line_number			=0;
	enum Level
	{
		OUTSIDE=0,GROUP=1,TECHNIQUE=2,PASS=3,TOO_FAR=4
	};
	Level level				=OUTSIDE;
	EffectTechnique *tech	=NULL;
	EffectPass *p			=NULL;
	string group_name,tech_name,pass_name;
	while(next>=0)
	{
		string line		=str.substr(pos,next-pos-1);
		base::ClipWhitespace(line);
		vector<string> words=simul::base::split(line,' ');
		pos				=next;
		int sp=line.find(" ");
		int open_brace=line.find("{");
		if(open_brace>=0)
			level=(Level)(level+1);
		string word;
		if(sp >= 0)
			word=line.substr(0, sp);
		if(level==OUTSIDE)
		{
			tech=nullptr;
			p=nullptr;
			if (is_equal(word, "group") )
			{
				group_name = line.substr(sp + 1, line.length() - sp - 1);
			}
			else if(is_equal(word,"texture"))
			{
				const string &texture_name	=words[1];
				const string &texture_dim	=words[2];
				const string &read_write	=words[3];
				const string &register_num	=words[4];
				string is_array				=words.size()>5?words[5]:"single";
				int slot=atoi(register_num.c_str());
				int dim=is_equal(texture_dim,"3d")?3:2;
				bool is_cubemap=is_equal(texture_dim,"cubemap");
				bool rw=is_equal(read_write,"read_write");
				bool ar=is_equal(is_array,"array");
				crossplatform::ShaderResource *tds=new crossplatform::ShaderResource;
				textureDetailsMap[texture_name]=tds;
				tds->slot				=slot;
				tds->dimensions			=dim;
				crossplatform::ShaderResourceType rt=crossplatform::ShaderResourceType::COUNT;
				if(!rw)
				{
					if(is_cubemap)
					{
							rt	=crossplatform::ShaderResourceType::TEXTURE_CUBE;
					}
					else
					{
						switch(dim)
						{
						case 1:
							rt	=crossplatform::ShaderResourceType::TEXTURE_1D;
							break;
						case 2:
							rt	=crossplatform::ShaderResourceType::TEXTURE_2D;
							break;
						case 3:
							rt	=crossplatform::ShaderResourceType::TEXTURE_3D;
							break;
						default:
							break;
						}
					}
				}
				else
				{
					switch(dim)
					{
					case 1:
						rt	=crossplatform::ShaderResourceType::RW_TEXTURE_1D;
						break;
					case 2:
						rt	=crossplatform::ShaderResourceType::RW_TEXTURE_2D;
						break;
					case 3:
						rt	=crossplatform::ShaderResourceType::RW_TEXTURE_3D;
						break;
					default:
						break;
					}
				}
				if(ar)
					rt=rt|crossplatform::ShaderResourceType::ARRAY;
				tds->shaderResourceType=rt;
			}
			else if(is_equal(word, "BlendState"))
			{
				string name		=words[1];
				string props	=words[2];
				size_t pos		=0;
				crossplatform::RenderStateDesc desc;
				desc.type=crossplatform::BLEND;
				desc.blend.AlphaToCoverageEnable=toBool(simul::base::toNext(props,',',pos));
				pos++;
				string enablestr=simul::base::toNext(props,')',pos);
				vector<string> en=base::split(enablestr,',');

				desc.blend.numRTs=en.size();
				pos++;
				crossplatform::BlendOperation BlendOp		=(crossplatform::BlendOperation)toInt(base::toNext(props,',',pos));
				crossplatform::BlendOperation BlendOpAlpha	=(crossplatform::BlendOperation)toInt(base::toNext(props,',',pos));
				crossplatform::BlendOption SrcBlend			=(crossplatform::BlendOption)toInt(base::toNext(props,',',pos));
				crossplatform::BlendOption DestBlend		=(crossplatform::BlendOption)toInt(base::toNext(props,',',pos));
				crossplatform::BlendOption SrcBlendAlpha	=(crossplatform::BlendOption)toInt(base::toNext(props,',',pos));
				crossplatform::BlendOption DestBlendAlpha	=(crossplatform::BlendOption)toInt(base::toNext(props,',',pos));
				pos++;
				string maskstr=base::toNext(props,')',pos);
				vector<string> ma=base::split(maskstr,',');

				for(int i=0;i<desc.blend.numRTs;i++)
				{
					bool enable=toBool(en[i]);
					desc.blend.RenderTarget[i].blendOperation				=enable?BlendOp:crossplatform::BLEND_OP_NONE;
					desc.blend.RenderTarget[i].blendOperationAlpha			=enable?BlendOpAlpha:crossplatform::BLEND_OP_NONE;
					if(desc.blend.RenderTarget[i].blendOperation!=crossplatform::BLEND_OP_NONE)
					{
						desc.blend.RenderTarget[i].SrcBlend					=SrcBlend;
						desc.blend.RenderTarget[i].DestBlend				=DestBlend;
					}
					else
					{
						desc.blend.RenderTarget[i].SrcBlendAlpha			=crossplatform::BLEND_ONE;
						desc.blend.RenderTarget[i].DestBlendAlpha			=crossplatform::BLEND_ZERO;
					}
					if(desc.blend.RenderTarget[i].blendOperationAlpha!=crossplatform::BLEND_OP_NONE)
					{
						desc.blend.RenderTarget[i].SrcBlendAlpha			=SrcBlendAlpha;
						desc.blend.RenderTarget[i].DestBlendAlpha			=DestBlendAlpha;
					}
					else
					{
						desc.blend.RenderTarget[i].SrcBlendAlpha			=crossplatform::BLEND_ONE;
						desc.blend.RenderTarget[i].DestBlendAlpha			=crossplatform::BLEND_ZERO;
					}
					desc.blend.RenderTarget[i].RenderTargetWriteMask	=(i<(int)ma.size())?toInt(ma[i]):0xF;
				}
				crossplatform::RenderState *bs=renderPlatform->CreateRenderState(desc);
				blendStates[name]=bs;
			}
			else if(is_equal(word, "RasterizerState"))
			{
				string name		=words[1];
				size_t pos		=0;
				crossplatform::RenderStateDesc desc;
				desc.type=crossplatform::RASTERIZER;
				// e.g. RenderBackfaceCull (false,CULL_BACK,0,0,false,FILL_WIREFRAME,true,false,false,0)
				vector<string> props=simul::base::split(words[2],',');
				//desc.rasterizer.antialias=toInt(props[0]);
				desc.rasterizer.cullFaceMode=toCullFadeMode(props[1]);
				desc.rasterizer.frontFace=toBool(props[6])?crossplatform::FRONTFACE_COUNTERCLOCKWISE:crossplatform::FRONTFACE_CLOCKWISE;
				desc.rasterizer.polygonMode=toPolygonMode(props[5]);
				desc.rasterizer.polygonOffsetMode=crossplatform::POLYGON_OFFSET_DISABLE;
				desc.rasterizer.viewportScissor=toBool(props[8])?crossplatform::VIEWPORT_SCISSOR_ENABLE:crossplatform::VIEWPORT_SCISSOR_DISABLE;
				/*
					0 AntialiasedLineEnable
					1 cullMode
					2 DepthBias
					3 DepthBiasClamp
					4 DepthClipEnable
					5 fillMode
					6 FrontCounterClockwise
					7 MultisampleEnable
					8 ScissorEnable
					9 SlopeScaledDepthBias
			*/
				crossplatform::RenderState *bs=renderPlatform->CreateRenderState(desc);
				rasterizerStates[name]=bs;
			}
			else if(is_equal(word, "DepthStencilState"))
			{
				string name		=words[1];
				string props	=words[2];
				size_t pos		=0;
				crossplatform::RenderStateDesc desc;
				desc.type=crossplatform::DEPTH;
				desc.depth.test=toBool(simul::base::toNext(props,',',pos));
				desc.depth.write=toInt(simul::base::toNext(props,',',pos));
				desc.depth.comparison=(crossplatform::DepthComparison)toInt(simul::base::toNext(props,',',pos));
				crossplatform::RenderState *ds=renderPlatform->CreateRenderState(desc);
				depthStencilStates[name]=ds;
			}
			else if(is_equal(word, "SamplerState"))
			{
				//SamplerState clampSamplerState 9,MIN_MAG_MIP_LINEAR,CLAMP,CLAMP,CLAMP,
				int sp2=line.find(" ",sp+1);
				string sampler_name = line.substr(sp + 1, sp2 - sp - 1);
				int comma=(int)std::min(line.length(),line.find(",",sp2+1));
				string register_num = line.substr(sp2 + 1, comma - sp2 - 1);
				int reg=atoi(register_num.c_str());
				simul::crossplatform::SamplerStateDesc desc;
				string state=line.substr(comma+1,line.length()-comma-1);
				vector<string> st=simul::base::split(state,',');
				desc.filtering=stringToFilter(st[0]);
				desc.x=stringToWrapping(st[1]);
				desc.y=stringToWrapping(st[2]);
				desc.z=stringToWrapping(st[3]);
				desc.slot=reg;
				crossplatform::SamplerState *ss=renderPlatform->GetOrCreateSamplerStateByName(sampler_name.c_str(),&desc);
				samplerStates[sampler_name]=ss;
			}
		}
		else if (level == GROUP)
		{
			if (sp >= 0 && _stricmp(line.substr(0, sp).c_str(), "technique") == 0)
			{
				tech_name = line.substr(sp + 1, line.length() - sp - 1);
				tech = (EffectTechnique*)EnsureTechniqueExists(group_name, tech_name, "main");
				p=nullptr;
			}
		}
		else if (level == TECHNIQUE)
		{
			if(sp>=0&&_stricmp(line.substr(0,sp).c_str(),"pass")==0)
			{
				pass_name=line.substr(sp+1,line.length()-sp-1);
				p=(EffectPass*)tech->AddPass(pass_name.c_str(),0);
			}
		}
		else if(level==PASS)
		{
			// Find the shader definitions:
			// vertex: simple_VS_Main_vv.sb
			// pixel: simple_PS_Main_p.sb
			int cl=line.find(":");
			int cm=line.find(",",cl+1);
			if(cm<0)
				cm=line.length();
			if(cl>=0&&tech)
			{
				string type=line.substr(0,cl);
				string filename=line.substr(cl+1,cm-cl-1);
				string uses;
				if(cm<line.length())
					uses=line.substr(cm+1,line.length()-cm-1);
				// textures,buffers,samplers
				vector<string> uses_tbs=simul::base::split(uses,',');
				base::ClipWhitespace(uses);
				base::ClipWhitespace(type);
				base::ClipWhitespace(filename);
				const string &name=words[1];
				crossplatform::ShaderType t=crossplatform::ShaderType::SHADERTYPE_COUNT;
				PixelOutputFormat fmt=FMT_32_ABGR;
				if(_stricmp(type.c_str(),"blend")==0)
				{
					if(blendStates.find(name)!=blendStates.end())
					{
						p->blendState=blendStates[name];
					}
					else
					{
						SIMUL_CERR<<"State not found: "<<name<<std::endl;
					}
				}
				else if(_stricmp(type.c_str(),"rasterizer")==0)
				{
					if(rasterizerStates.find(name)!=rasterizerStates.end())
					{
						p->rasterizerState=rasterizerStates[name];
					}
					else
					{
						SIMUL_CERR<<"Rasterizer state not found: "<<name<<std::endl;
					}
				}
				else if(_stricmp(type.c_str(),"depthstencil")==0)
				{
					if(depthStencilStates.find(name)!=depthStencilStates.end())
					{
						p->depthStencilState=depthStencilStates[name];
					}
					else
					{
						SIMUL_CERR<<"Depthstencil state not found: "<<name<<std::endl;
					}
				}
				else
				{
					if(_stricmp(type.c_str(),"vertex")==0)
						t=crossplatform::SHADERTYPE_VERTEX;
					else if(_stricmp(type.c_str(),"export")==0)
						t=crossplatform::SHADERTYPE_VERTEX;
					else if(_stricmp(type.c_str(),"geometry")==0)
						t=crossplatform::SHADERTYPE_GEOMETRY;
					else if(_stricmp(type.substr(0,5).c_str(),"pixel")==0)
					{
						t=crossplatform::SHADERTYPE_PIXEL;
						if(type.length()>6)
						{
							string out_fmt=type.substr(6,type.length()-7);
							if(_stricmp(out_fmt.c_str(),"float16abgr")==0)
								fmt=FMT_FP16_ABGR;
							else if(_stricmp(out_fmt.c_str(),"float32abgr")==0)
								fmt=FMT_32_ABGR;
							else if(_stricmp(out_fmt.c_str(),"snorm16abgr")==0)
								fmt=FMT_SNORM16_ABGR;
							else if(_stricmp(out_fmt.c_str(),"unorm16abgr")==0)
								fmt=FMT_UNORM16_ABGR;
						}
					}
					else if(_stricmp(type.c_str(),"compute")==0)
						t=crossplatform::SHADERTYPE_COMPUTE;
					else
					{
						SIMUL_BREAK(base::QuickFormat("Unknown shader type or command: %s\n",type.c_str()));
						continue;
					}
					Shader *s=renderPlatform->EnsureShader(filename.c_str(),t);
					if(s)
					{
						if(t==crossplatform::SHADERTYPE_PIXEL)
							p->pixelShaders[fmt]=s;
						else
							p->shaders[t]=s;
					}
					// Now we will know which slots must be used by the pass:
					p->SetUsesBufferSlots(s->bufferSlots);
					p->SetUsesTextureSlots(s->textureSlots);
					p->SetUsesTextureSlotsForSB(s->textureSlotsForSB);
					p->SetUsesRwTextureSlots(s->rwTextureSlots);
					p->SetUsesRwTextureSlotsForSB(s->rwTextureSlotsForSB);
					p->SetUsesSamplerSlots(s->samplerSlots);
				}
			}
		}
		int close_brace=line.find("}");
		if (close_brace >= 0)
		{
			level = (Level)(level - 1);
			if (level == OUTSIDE)
				group_name = "";
		}
		next			=(int)str.find('\n',pos+1);
	}
	SIMUL_ASSERT(level==OUTSIDE);
	simul::base::FileLoader::GetFileLoader()->ReleaseFileContents(ptr);
	
}
void Shader::setUsesTextureSlot(int s)
{
	unsigned m=((unsigned)1<<(unsigned)s);
	if((textureSlotsForSB&m)!=0)
	{
		SIMUL_BREAK_ONCE("Can't use slot for Texture and Structured Buffer in the same shader.");
	}
	textureSlots=textureSlots|m;
}
void Shader::setUsesTextureSlotForSB(int s)
{
	unsigned m=((unsigned)1<<(unsigned)s);
	if((textureSlots&m)!=0)
	{
		SIMUL_BREAK_ONCE("Can't use slot for Texture and Structured Buffer in the same shader.");
	}
	textureSlotsForSB=textureSlotsForSB|m;
}

void Shader::setUsesBufferSlot(int s)
{
	bufferSlots=bufferSlots|((unsigned)1<<(unsigned)s);
}

void Shader::setUsesSamplerSlot(int s)
{
	samplerSlots=samplerSlots|((unsigned)1<<(unsigned)s);
}

void Shader::setUsesRwTextureSlot(int s)
{
	unsigned m=((unsigned)1<<(unsigned)s);
	rwTextureSlots=rwTextureSlots|m;
}

void Shader::setUsesRwTextureSlotForSB(int s)
{
	unsigned m=((unsigned)1<<(unsigned)s);
	if((rwTextureSlots&m)!=0)
	{
		SIMUL_BREAK_ONCE("Can't use slot for RW Texture and Structured Buffer in the same shader.");
	}
	rwTextureSlotsForSB=rwTextureSlotsForSB|m;
}

bool Shader::usesTextureSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (textureSlots&m)!=0;
}

bool Shader::usesTextureSlotForSB(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (textureSlotsForSB&m)!=0;
}

bool Shader::usesRwTextureSlotForSB(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (rwTextureSlotsForSB&m)!=0;
}

bool Shader::usesBufferSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (bufferSlots&m)!=0;
}

bool Shader::usesSamplerSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return true;//(samplerSlots&m)!=0;
}

bool Shader::usesRwTextureSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (rwTextureSlots&m)!=0;
}