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
#include <thread>
#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <atomic>

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
  Vector location;
  Vector speed;
  char* name;
  int health;

  static constexpr int NUM_TYPES = 4;
  enum class Type { Zombie,Chicken,Exploder,TallCreepyThing };
  Entity(Vector, Type);
  Entity() = default;
  ~Entity() = default;
  void updatePosition();
};

Entity::Entity (Vector location,Type type) {
  this->location = location;

  switch(type)
  {
    case Type::Zombie:
      name = "Zombie";
      health = 50;
      speed = Vector(0.5,0.0,0.5);
      break;
    case Type::Chicken:
      name = "Checking";
      health = 25;
      speed = Vector(0.75,0.25,0.75);
      break;
    case Type::Exploder:
      name = "Exploder";
      health = 75;
      speed = Vector(0.75,0.0,0.75);
      break;
    case Type::TallCreepyThing:
      name ="Tall Creepy Thing";
      health = 500;
      speed = Vector(1.0,1.0,1.0);
      break;
  }
}

void Entity::updatePosition() {
  new (&location) Vector(location.x + 1.0f * speed.x,
                         location.y + 1.0f * speed.y,
                         location.z + 1.0f * speed.z);
}


class Chunk {
  static constexpr int NUM_BLOCKS = 65536;
  static constexpr int NUM_ENTITIES = 1000;

public:
  std::array<unsigned char, NUM_BLOCKS> blocks;
  std::vector<Entity> entities;

  Vector location;
  Chunk(Vector);
  Chunk() = default;
  ~Chunk() = default;
  void processEntities();
};

Chunk::Chunk(Vector location) {
  this->location = location;
  for (int i = 0; i < NUM_BLOCKS; i+=4) {
    blocks[i] = i%256;
    blocks[i+1] = (i+1)%256;
    blocks[i+2] = (i+2)%256;
    blocks[i+3] = (i+3)%256;
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
  static constexpr int BLOCK_COUNT = 256;
  static constexpr int CHUNK_COUNT = 100;
  std::vector<Block> blocks;
  std::vector<Chunk> chunks;
  Vector playerLocation;
  std::atomic_uint chunkCounter;
  Game();
  void loadWorld();
  void updateChunks();
  static void update(Chunk& chunk,
                     const Vector playerLocation,
                     std::atomic_uint& chunkCounter);
};

Game::Game() : playerLocation({0, 0, 0}) {
  blocks.reserve(BLOCK_COUNT);
  for (int i = 0; i < BLOCK_COUNT; i+=4) {
    blocks.emplace_back("Block" + std::to_string(i), Vector(i,i,i), i, 100, 1, 1, true, true);
    blocks.emplace_back("Block" + std::to_string(i+1), Vector(i+1,i+1,i+1), i+1, 100, 1, 1, true, true);
    blocks.emplace_back("Block" + std::to_string(i+2), Vector(i+2,i+2,i+2), i+2, 100, 1, 1, true, true);
    blocks.emplace_back("Block" + std::to_string(i+3), Vector(i+3,i+3,i+3), i+3, 100, 1, 1, true, true);
  }

  chunkCounter = 0;
}

void Game::loadWorld() {
  chunks.reserve(CHUNK_COUNT);
  //Doesn't seem to improve performance enormous, but what the heck
  //If i don't parallelize I can switch to emplace
#pragma omp parallel for
  for (int i = 0; i < CHUNK_COUNT;i+=4) {
    new (&chunks[i]) Chunk(Vector(chunkCounter++, 0.0, 0.0));
    new (&chunks[i+1]) Chunk(Vector(chunkCounter++, 0.0, 0.0));
    new (&chunks[i+2]) Chunk(Vector(chunkCounter++, 0.0, 0.0));
    new (&chunks[i+3]) Chunk(Vector(chunkCounter++, 0.0, 0.0));
  }
}

void Game::update(Chunk& chunk,
                  const Vector playerLocation,
                  std::atomic_uint& chunkCounter) {
  chunk.processEntities();
  float chunkDistance = Vector::getDistance(chunk.location, playerLocation);
  if (chunkDistance > CHUNK_COUNT)
    new (&chunk) Chunk(Vector(chunkCounter++,0.0,0.0));
}

void Game::updateChunks() {
#pragma omp parallel for
  for (int i = 0; i < CHUNK_COUNT; i+=4) {
    Game::update(chunks[i], playerLocation, chunkCounter);
    Game::update(chunks[i+1], playerLocation, chunkCounter);
    Game::update(chunks[i+2], playerLocation, chunkCounter);
    Game::update(chunks[i+3], playerLocation, chunkCounter);
  }
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
  //spin
  int i = 0;
  double dur = 0;
  while(1) {
    start = high_resolution_clock::now();
    Vector playerMovement = Vector(0.1,0.0,0.0);

    game->playerLocation = Vector::add(playerMovement,game->playerLocation);
    game->updateChunks();

    end = high_resolution_clock::now();

    dur += (duration_cast<nanoseconds>(end-start).count() / 1000000.0);

    if ((++i)%1000 == 0)
      printf("%f\n", dur/(double)i);
  }
}
