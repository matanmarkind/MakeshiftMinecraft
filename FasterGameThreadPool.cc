/*
 * An optimization mainly to avoid memory reallocations during gameplay.
 * Also unspooled the loops, based on promise of divisible by 4.
 *
 * Article - Taking out the garbage, Jack Mott, jack.mott@gmail.com
 * https://jackmott.github.io/programming/2016/09/01/performance-in-the-large.html
 *
 * Code directly based on https://gist.github.com/jcelerier/f6b666041162eb221bfca441eccb0ee7
 */

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <random>
#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <atomic>
#include "matan/ThreadPool.hh"

using namespace std;
using namespace std::chrono;

class Vector {
public:
  float x, y, z;
  Vector() = default;
  ~Vector() = default;
  Vector(float x_, float y_, float z_): x(x_), y(y_), z(z_) { }

  static Vector add(const Vector a, const Vector b) {
    return Vector(a.x+b.x,a.y+b.y,a.z+b.z);
  }

  static Vector mul(const Vector a, const Vector b) {
    return Vector(a.x*b.x,a.y*b.y,a.z*b.z);
  }
  static Vector sub(const Vector a, const Vector b) {
    return Vector(a.x-b.x,a.y-b.y,a.z-b.z);
  }
  static float getDistance(const Vector a, const Vector b) {
    Vector s = Vector::sub(a,b);
    return (float) std::sqrt(s.x*s.x + s.y*s.y + s.z*s.z);
  }
};

class Block {
public:
  std::string name;
  Vector m_location;
  int durability;
  int textureid;
  int type;
  unsigned char id;
  bool breakable;
  bool visible;

  Block() = default;
  Block(std::string n,
        Vector location,
        unsigned char id,
        int durability,
        int textureid,
        int type,
        bool breakable,
        bool visible):
          name(std::move(n)),
          m_location(location),
          durability(durability),
          textureid(textureid),
          type(type),
          id(id),
          breakable(breakable),
          visible(visible){}
  ~Block() = default;
};


class Entity {
public:
  enum class Type { Zombie,Chicken,Exploder,TallCreepyThing };

  static constexpr int NUM_TYPES = 4;
  Vector m_location;
  const char* name;
  Vector speed;
  int health;

  Entity(Vector, Type);
  Entity() = default;
  ~Entity() = default;
  void updatePosition();
  static const char* get_name(Type type);
};

const char* Entity::get_name(Type type) {
  switch(type)
  {
    case Type::Zombie:
      return "Zombie";
    case Type::Chicken:
      return"Chicken";
    case Type::Exploder:
      return "Exploder";
    case Type::TallCreepyThing:
      return "Tall Creepy Thing";
  }
}

Entity::Entity (Vector location,Type type) :
    m_location(location),
    name(Entity::get_name(type)) {
  switch(type)
  {
    case Type::Zombie:
      health = 50;
      speed = Vector(0.5,0.0,0.5);
      break;
    case Type::Chicken:
      health = 25;
      speed = Vector(0.75,0.25,0.75);
      break;
    case Type::Exploder:
      health = 75;
      speed = Vector(0.75,0.0,0.75);
      break;
    case Type::TallCreepyThing:
      health = 500;
      speed = Vector(1.0,1.0,1.0);
      break;
  }
}

void Entity::updatePosition() {
  m_location.x = m_location.x + 1.0f * speed.x;
  m_location.y = m_location.y + 1.0f * speed.y;
  m_location.z = m_location.z + 1.0f * speed.z;
}


class Chunk {
  static constexpr int NUM_BLOCKS = 65536;
  static constexpr int NUM_ENTITIES = 1000;

public:
  std::vector<unsigned char> m_blocks;
  std::vector<Entity> entities;
  Vector m_location;

  Chunk(Vector);
  Chunk() = default;
  ~Chunk() = default;
  void processEntities();
};

Chunk::Chunk(Vector location) {
  m_location = location;
  m_blocks.reserve(NUM_BLOCKS);
  for (int i = 0; i < NUM_BLOCKS; i+=4) {
    m_blocks[i] = i%256;
    m_blocks[i+1] = (i+1)%256;
    m_blocks[i+2] = (i+2)%256;
    m_blocks[i+3] = (i+3)%256;
  }

  entities.reserve(NUM_ENTITIES);
  const auto beg = entities.begin();
  for (int i = 0; i < NUM_ENTITIES; i+=4) {
    entities.emplace(beg+i, Vector(i,i,i), Entity::Type::Zombie);
    entities.emplace(beg+i+1, Vector(i+1,i+1,i+1), Entity::Type::Chicken);
    entities.emplace(beg+i+2, Vector(i+2,i+2,i+2), Entity::Type::Exploder);
    entities.emplace(beg+i+3, Vector(i+3,i+3,i+3), Entity::Type::TallCreepyThing);
  }
}

void Chunk::processEntities() {
  for (int i = 0; i < NUM_ENTITIES; i+=4) {
    entities[i].updatePosition();
    entities[i+1].updatePosition();
    entities[i+2].updatePosition();
    entities[i+3].updatePosition();
  }
}

class Game {
public:
  static constexpr int BLOCKS_COUNT = 256;
  static constexpr int CHUNKS_COUNT = 100;
  Block m_blocks[BLOCKS_COUNT];
  Chunk m_chunks[CHUNKS_COUNT];
  Vector playerLocation;
  int m_chunkCounter;
  matan::ThreadPool m_threadPool;

  Game();
  void loadWorld();
  void updateChunks();
  static void update(Chunk& chunk,
                     const Vector playerLocation,
                     int chunkCounter);
};

Game::Game() :
    playerLocation({0, 0, 0}),
    m_threadPool(2) {
  for (int i = 0; i < BLOCKS_COUNT; i+=4) {
    new (m_blocks+i) Block("Block" + std::to_string(i), Vector(i,i,i), i, 100, 1, 1, true, true);
    new (m_blocks+i+1) Block("Block" + std::to_string(i+1), Vector(i+1,i+1,i+1), i+1, 100, 1, 1, true, true);
    new (m_blocks+i+2) Block("Block" + std::to_string(i+2), Vector(i+2,i+2,i+2), i+2, 100, 1, 1, true, true);
    new (m_blocks+i+3) Block("Block" + std::to_string(i+3), Vector(i+3,i+3,i+3), i+3, 100, 1, 1, true, true);
  }

  m_chunkCounter = 0;
}

void Game::loadWorld() {
  for (int i = 0; i < CHUNKS_COUNT; i+=4) {
    new (m_chunks+i) Chunk(Vector(m_chunkCounter++, 0.0, 0.0));
    new (m_chunks+i+1) Chunk(Vector(m_chunkCounter++, 0.0, 0.0));
    new (m_chunks+i+2) Chunk(Vector(m_chunkCounter++, 0.0, 0.0));
    new (m_chunks+i+3) Chunk(Vector(m_chunkCounter++, 0.0, 0.0));
  }
}

void Game::update(Chunk& chunk,
                  const Vector playerLocation,
                  int chunkCounter) {
  chunk.processEntities();
  float chunkDistance = Vector::getDistance(chunk.m_location, playerLocation);
  if (chunkDistance > CHUNKS_COUNT) {
    chunk.~Chunk();
    new (&chunk) Chunk(Vector(chunkCounter,0.0,0.0));
    //chunk = Chunk(Vector(chunkCounter,0.0,0.0));
  }
}

void Game::updateChunks() {
  for (int i = 0; i < CHUNKS_COUNT; i+=4) {
    m_threadPool.enqueue(Game::update, m_chunks[i], playerLocation, m_chunkCounter++);
    m_threadPool.enqueue(Game::update, m_chunks[i+1], playerLocation, m_chunkCounter++);
    m_threadPool.enqueue(Game::update, m_chunks[i+2], playerLocation, m_chunkCounter++);
    m_threadPool.enqueue(Game::update, m_chunks[i+3], playerLocation, m_chunkCounter++);
  }
  m_threadPool.waitFinished();
}

int main(int argc, char* argv[]) {
  Game game;
  printf("%lu\n", sizeof(Game));
  high_resolution_clock::time_point start;
  high_resolution_clock::time_point end;

  printf("loading world...\n");
  start = high_resolution_clock::now();
  game.loadWorld();
  end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end-start).count();
  printf("load time:%lu\n",duration);
  //spin
  int i = 0;
  double dur = 0;
  while(1) {
    start = high_resolution_clock::now();
    Vector playerMovement = Vector(0.1,0.0,0.0);

    game.playerLocation = Vector::add(playerMovement, game.playerLocation);
    game.updateChunks();

    end = high_resolution_clock::now();

    dur += (duration_cast<nanoseconds>(end-start).count() / 1000000.0);

    if ((++i)%1000 == 0)
      printf("%f\n", dur/(double)i);
  }
}
