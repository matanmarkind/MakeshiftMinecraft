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
#include <thread>
#include <cmath>
#include <string>
#include <array>
#include "ThreadPool.hh"
#include "memory.hh"

using namespace std;
using namespace std::chrono;

class Vector {
public:
  float x, y, z;
  Vector() = default;
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
  Vector location;
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
          location(location),
          durability(durability),
          textureid(textureid),
          type(type),
          id(id),
          breakable(breakable),
          visible(visible){}
};

class Entity {
public:
  enum class Type { Zombie,Chicken,Exploder,TallCreepyThing };

  Vector m_location;
  Vector speed;
  const char* name;
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
public:
  std::array<unsigned char, 65536> blocks;
  std::array<Entity, 1000> entities;
  Vector location;

  Chunk(Vector);
  Chunk(float x, float y, float z);
  Chunk() = default;
  ~Chunk() = default;
  void init();
  void processEntities();
};

Chunk::Chunk(Vector loc) :
  location(loc) {
  init();
}

Chunk::Chunk(float x, float y, float z) :
  location(x, y, z) {
  init();
}

void Chunk::init() {
  for (int i = 0; i < blocks.size(); i+=4) {
    blocks[i] = i%256;
    blocks[i+1] = (i+1)%256;
    blocks[i+2] = (i+2)%256;
    blocks[i+3] = (i+3)%256;
  }

  for (int i = 0; i < entities.size(); i+=4) {
    matan::place(&entities[i], Vector(i,i,i), Entity::Type::Zombie);
    matan::place(&entities[i+1], Vector(i+1,i+1,i+1), Entity::Type::Chicken);
    matan::place(&entities[i+2], Vector(i+2,i+2,i+2), Entity::Type::Exploder);
    matan::place(&entities[i+3], Vector(i+3,i+3,i+3), Entity::Type::TallCreepyThing);
  }
}

void Chunk::processEntities() {
  for (int i = 0; i < entities.size(); i+=4) {
    entities[i].updatePosition();
    entities[i+1].updatePosition();
    entities[i+2].updatePosition();
    entities[i+3].updatePosition();
  }
}

class Game {
public:
  static constexpr int CHUNK_COUNT = 100;
  std::array<Block, 256> blocks;
  std::array<Chunk, CHUNK_COUNT> chunks;
  Vector playerLocation;
  std::atomic_uint chunkCounter;
  matan::ThreadPool m_threadPool;
  Game();
  void loadWorld();
  void updateChunks();
  static void update(Chunk& chunk,
                     const Vector playerLocation,
                     std::atomic_uint& chunkCounter);
};

Game::Game() :
    playerLocation({0, 0, 0}),
    m_threadPool() {
  for (int i = 0; i < blocks.size(); i+=4) {
    matan::place(&blocks[i], "Block" + std::to_string(i), Vector(i,i,i), i, 100, 1, 1, true, true);
    matan::place(&blocks[i+1], "Block" + std::to_string(i+1), Vector(i+1,i+1,i+1), i+1, 100, 1, 1, true, true);
    matan::place(&blocks[i+2], "Block" + std::to_string(i+2), Vector(i+2,i+2,i+2), i+2, 100, 1, 1, true, true);
    matan::place(&blocks[i+3], "Block" + std::to_string(i+3), Vector(i+3,i+3,i+3), i+3, 100, 1, 1, true, true);
  }

  chunkCounter = 0;
}

void Game::loadWorld() {
  for (int i = 0; i < chunks.size(); i+=4) {
    matan::place(&chunks[i], Vector(chunkCounter++, 0.0, 0.0));
    matan::place(&chunks[i+1], Vector(chunkCounter++, 0.0, 0.0));
    matan::place(&chunks[i+2], Vector(chunkCounter++, 0.0, 0.0));
    matan::place(&chunks[i+3], Vector(chunkCounter++, 0.0, 0.0));
  }
}

void Game::update(Chunk& chunk,
                  const Vector playerLocation,
                  std::atomic_uint& chunkCounter) {
  chunk.processEntities();
  if (Vector::getDistance(chunk.location, playerLocation) > CHUNK_COUNT) {
    matan::replace(&chunk, chunkCounter++, 0, 0);
  }
}

void Game::updateChunks() {
  for (int i = 0; i < chunks.size(); i+=4) {
    m_threadPool.enqueue(Game::update, chunks[i], playerLocation, chunkCounter);
    m_threadPool.enqueue(Game::update, chunks[i+1], playerLocation, chunkCounter);
    m_threadPool.enqueue(Game::update, chunks[i+2], playerLocation, chunkCounter);
    m_threadPool.enqueue(Game::update, chunks[i+3], playerLocation, chunkCounter);
  }
  m_threadPool.waitFinished();
}

int main(int argc, char* argv[]) {
  auto game = new Game;
  printf("%lu\n", sizeof(Game));
  high_resolution_clock::time_point start;
  high_resolution_clock::time_point end;

  printf("loading world...\n");
  start = high_resolution_clock::now();
  game->loadWorld();
  end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end-start).count();
  printf("load time:%lu\n",duration);

  int i = 0;
  double dur = 0;
  const Vector playerMovement = Vector(0.1,0.0,0.0);
  while(1) {
    start = high_resolution_clock::now();
    game->playerLocation = Vector::add(playerMovement, game->playerLocation);
    game->updateChunks();
    end = high_resolution_clock::now();

    auto duration = (double)(duration_cast<nanoseconds>(end-start).count() / 1000000.0);
    printf("%f\n",duration);
    if(duration < 16) {
      this_thread::sleep_for(milliseconds((long)(16.0-duration)));
    }

    /*
    dur += (duration_cast<nanoseconds>(end-start).count() / 1000000.0);
    if ((++i)%1000 == 0) {
      printf("%f\n", dur/(double)i);
    }
    */
  }
}
