#include "Map.hpp"
#include <fstream>
#include <string>
#include <glm/gtc/matrix_transform.hpp>

Map::Map(std::string const & map, kit::RenderPayload::Ptr renderPayload)
{
  this->m_renderPayload = renderPayload;
  this->loadHeader(map);
  this->loadData(map);
  this->updateBlocks();
  std::cout << "Map size is " << this->m_size.x << "x" << this->m_size.y << std::endl;
}

Map::~Map()
{
}

Map::Ptr Map::load(std::string const & map, kit::RenderPayload::Ptr renderPayload)
{
  return std::make_shared<Map>(map, renderPayload);
}

void Map::loadHeader(std::string const & map)
{
  std::ifstream f(std::string("./data/maps/") + map + std::string("/header"));
  if (!f)
  {
    KIT_ERR("Failed to open map header");
  }

  std::string currline = "";
  while (std::getline(f, currline))
  {
    std::vector<std::string> currtokens = kit::splitString(currline);

    if (currtokens.size() != 0)
    {
      if ("size" == currtokens[0] && currtokens.size() == 3)
      {
        this->m_size.x = std::atoi(currtokens[1].c_str());
        this->m_size.y = std::atoi(currtokens[2].c_str());

        // Reserve data
        this->m_tiles.insert(this->m_tiles.begin(), this->m_size.y, std::vector<MapTile>(this->m_size.x));
      }

      if ("hardblock" == currtokens[0] && currtokens.size() == 2)
      {
        this->m_hardBlocks = kit::Model::create(std::string(currtokens[1]));
        this->m_renderPayload->addRenderable(this->m_hardBlocks);
      }

      if ("softblock" == currtokens[0] && currtokens.size() == 2)
      {
        this->m_softBlocks = kit::Model::create(std::string(currtokens[1]));
        this->m_renderPayload->addRenderable(this->m_softBlocks);
      }
      
      if ("terrain" == currtokens[0] && currtokens.size() == 2)
      {
        this->m_terrain = kit::BakedTerrain::load(std::string(currtokens[1]));
        this->m_terrain->translate(glm::vec3(float(this->m_size.x) / 2.0f - 0.5f, 0.0f, float(this->m_size.y)  / 2.0f - 0.5f));
        this->m_terrain->setDetailDistance(20.0f);
        this->m_renderPayload->addRenderable(this->m_terrain);
      }
    }
  }
  f.close();
}

void Map::loadData(std::string const & map)
{
  std::ifstream f(std::string("./data/maps/") + map + std::string("/data"), std::ios::in | std::ios_base::binary);
  if (!f)
  {
    KIT_ERR("Failed to open map data");
  }

  for(int32_t y = 0; y < this->m_size.y; y++)
  {
    for(int32_t x = 0; x < this->m_size.x; x++)
    {
      this->m_tiles.at(y).at(x).type = (MapTile::Type)kit::readUint8(f);
      this->m_tiles.at(y).at(x).probability = float(kit::readUint8(f)) / 255.0f;
      this->m_tiles.at(y).at(x).transform = glm::rotate(
        glm::translate(glm::mat4(), glm::vec3((float)x, 0.0f, (float)y)),
        glm::radians(float(kit::randomInt(0, 3))*90.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
      );
    }
  }

  f.close();
}

void Map::update(double const & ms)
{
}

void Map::updateBlocks()
{
  static std::vector<glm::mat4> transformSoft;
  static std::vector<glm::mat4> transformHard;

  transformSoft.clear();
  transformHard.clear();

  for(auto & currRow : this->m_tiles)
  {
    for(auto & currCol : currRow)
    {
      if(currCol.type == MapTile::Type::Soft)
      {
        transformSoft.push_back(currCol.transform);
      }
      else if(currCol.type == MapTile::Type::Hard)
      {
        transformHard.push_back(currCol.transform);
      }
    }
  }

  this->m_softBlocks->setInstancing(true, transformSoft);
  this->m_hardBlocks->setInstancing(true, transformHard);
}

void Map::applyExplosion(glm::ivec2 origin, uint8_t range)
{
  if(origin.x < 0 || origin.x >= this->m_size.x || origin.y < 0 || origin.y >= this->m_size.y)
  {
    std::cout << "Explosion origin out of bounds";
  }
  
  // Blow up the origin tile
  if(this->m_tiles.at(origin.y).at(origin.x).type == MapTile::Type::Soft)
  {
    this->m_tiles.at(origin.y).at(origin.x).type = MapTile::Type::Empty;
  }

  // Implement a lambda func that traces in a direction, blowing up blocks
  auto trace = [this, origin, range](glm::ivec2 direction){
    glm::ivec2 traceOrigin = origin + direction;
    for(uint8_t i = 0; i < range; i++)
    {
      if(traceOrigin.x < 0 || traceOrigin.x >= this->m_size.x || traceOrigin.y < 0 || traceOrigin.y >= this->m_size.y)
      {
        break;
      }

      if(this->m_tiles.at(traceOrigin.y).at(traceOrigin.x).type == MapTile::Type::Hard)
      {
        break;
      }

      this->m_tiles.at(traceOrigin.y).at(traceOrigin.x).type = MapTile::Type::Empty;
      traceOrigin += direction;
    }
  };

  // Trace right, left, down, up
  trace(glm::ivec2(0, 1));
  trace(glm::ivec2(0, -1));
  trace(glm::ivec2(1, 0));
  trace(glm::ivec2(-1, 0));
  
  this->updateBlocks();
}

glm::ivec2 Map::worldToGrid(glm::vec3 const & position)
{
  glm::vec3 inPosition = position;
  auto align = [](float value, int32_t size)->int32_t{
    return (value/size)*size;
  };
  
  inPosition.x = glm::clamp(inPosition.x+0.5f, 0.0f, (float)this->m_size.x - 1.0f);
  inPosition.z = glm::clamp(inPosition.z+0.5f, 0.0f, (float)this->m_size.y - 1.0f);
  
  return glm::ivec2(align(inPosition.x, this->m_size.x), align(inPosition.z, this->m_size.y));
}

glm::vec3 Map::gridToWorld(glm::ivec2 const & grid, float const & height)
{
  glm::vec3 returner;
  returner.y = height;
  returner.x = float(grid.x);
  returner.z = float(grid.y);
  return returner;
}

MapTile const & Map::getTile(glm::ivec2 const & grid)
{
  glm::ivec2 in;
  in.x = glm::clamp(grid.x, 0, this->m_size.x - 1);
  in.y = glm::clamp(grid.y, 0, this->m_size.y - 1);
  
  return this->m_tiles.at(in.y).at(in.x);
}

glm::ivec2 Map::getSize()
{
  return this->m_size;
}