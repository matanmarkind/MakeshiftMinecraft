// clarity.cpp : Defines the entry point for the console application.
/*
 * Changes
 *  - switch to constexpr where possible
 *  - switch arrays to vector
 *  - make Entity and Block constant size, so can allocate all the memory required at once.
 *      removed names, seemed useless and if I want a unique identifier I can use an int ID. This part has no need to be human readable.
 *  - unspooled loops. Need to guarantee all arrays/vectors are divisible by 4
 *
 *  A much smaller change would be to get the space for all the 4 types of entities and then that is a constant since we know how many Entities of each type.
 */
#include <stdio.h>
#include <chrono>
#include <random>
#include <vector>
#include <iostream>
#include "/home/matan/ClionProjects/matan/ThreadPool.hh"

using namespace std;

class Vector {
public:
  float x, y, z;

  Vector();

  Vector(float, float, float);

  static Vector add(Vector a, Vector b);

  static Vector mul(Vector a, Vector b);

  static Vector sub(Vector a, Vector b);

  static float getDistance(Vector a, Vector b);
};

Vector::Vector() {
  x = 0;
  y = 0;
  z = 0;
}


Vector::Vector(float x, float y, float z) {
  this->x = x;
  this->y = y;
  this->z = z;
}

float Vector::getDistance(Vector a, Vector b) {
  Vector s = Vector::sub(a, b);
  float result = (float) sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
  return result;
}

Vector Vector::add(Vector a, Vector b) {
  return Vector(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vector Vector::mul(Vector a, Vector b) {
  return Vector(a.x * b.x, a.y * b.y, a.z * b.z);
}

Vector Vector::sub(Vector a, Vector b) {
  return Vector(a.x - b.x, a.y - b.y, a.z - b.z);
}

class Block {
public:
  Vector location;
  int durability;
  int textureid;
  bool breakable;
  bool visible;
  int type;
  unsigned char id;

  Block();

  Block(unsigned char id, Vector, int, int, bool, bool, int);

  ~Block();
};

Block::Block() {}

Block::Block(unsigned char id, Vector location, int durability, int textureid,
             bool breakable, bool visible, int type) {
  this->id = id;
  this->location = location;
  this->textureid = textureid;
  this->breakable = breakable;
  this->visible = visible;
  this->type = type;
}

Block::~Block() {}

class Entity {
public:
  Vector location;
  int health;
  Vector speed;
  enum class Type {
    Zombie, Chicken, Exploder, TallCreepyThing
  };

  Entity() {};

  Entity(Vector, Type);

  ~Entity();

  void updatePosition();
};

Entity::Entity(Vector location, Type type) {
  this->location = location;

  switch (type) {
    case Type::Zombie:
      health = 50;
      speed = Vector(0.5, 0.0, 0.5);
      break;
    case Type::Chicken:
      health = 25;
      speed = Vector(0.75, 0.25, 0.75);
      break;
    case Type::Exploder:
      health = 75;
      speed = Vector(0.75, 0.0, 0.75);
      break;
    case Type::TallCreepyThing:
      health = 500;
      speed = Vector(1.0, 1.0, 1.0);
      break;
  }
}

Entity::~Entity() {}

void Entity::updatePosition() {
  location = Vector(location.x + 1.0f * speed.x, location.y + 1.0f * speed.y,
                    location.z + 1.0f * speed.z);
}

class Chunk {
  static constexpr size_t NUM_BLOCKS = 65536;
  static constexpr size_t NUM_ENTITIES = 1000;

public:
  vector<unsigned char> blocks;
  vector<Entity> entities;
  Vector location;

  Chunk() { Chunk(Vector(0, 0, 0)); };

  Chunk(Vector);

  ~Chunk();

  void processEntities();
};


Chunk::Chunk(Vector location) :
        blocks(NUM_BLOCKS) {
  entities.reserve(NUM_ENTITIES);
  this->location = location;
  unsigned int i = 0;
  while (i < NUM_BLOCKS) {
    blocks[i] = i % 256;
    i++;
    blocks[i] = i % 256;
    i++;
    blocks[i] = i % 256;
    i++;
    blocks[i] = i % 256;
    i++;
  }

  i = 0;
  while (i < NUM_ENTITIES) {
    entities.emplace_back(Vector(i, i, i), Entity::Type::Chicken);
    i++;
    entities.emplace_back(Vector(i, i, i), Entity::Type::Chicken);
    i++;
    entities.emplace_back(Vector(i, i, i), Entity::Type::Chicken);
    i++;
    entities.emplace_back(Vector(i, i, i), Entity::Type::Chicken);
    i++;
  }
}

Chunk::~Chunk() {}

void Chunk::processEntities() {
  for (auto &entity : entities) {
    entity.updatePosition();
  }
}


class Game {
public:
  Game();
  ~Game();
  void loadWorld();
  void updateChunks();

  Vector playerLocation;

private:
  static constexpr size_t CHUNK_COUNT = 100;
  static constexpr size_t BLOCK_COUNT = 256;
  vector<Block> blocks;
  Chunk* chunks[CHUNK_COUNT];
  std::atomic_uint chunkCounter;
  matan::ThreadPool m_threadPool;
  std::mutex m_mtx;

  std::atomic_uint m_process;
  std::atomic_uint m_distance;
  std::atomic_uint m_replace;

  void update(Chunk* chunk, const Vector playerLocation);
};

Game::Game() : m_process(0), m_distance(0), m_replace(0) {
  blocks.reserve(BLOCK_COUNT);
  unsigned int i = 0;
  while (i < BLOCK_COUNT) {
    blocks.emplace_back(i, Vector(i, i, i), 100, 1, true, true, 1);
    i++;
    blocks.emplace_back(i, Vector(i, i, i), 100, 1, true, true, 1);
    i++;
    blocks.emplace_back(i, Vector(i, i, i), 100, 1, true, true, 1);
    i++;
    blocks.emplace_back(i, Vector(i, i, i), 100, 1, true, true, 1);
    i++;
  }

  chunkCounter = 0;
  playerLocation = Vector(0, 0, 0);
}

Game::~Game() {
  for (auto c : chunks) {
    delete c;
  }
}

void Game::loadWorld() {
  int i = 0;
  while (i < CHUNK_COUNT) {
    chunks[i++] = new Chunk(Vector(chunkCounter++, 0.0, 0.0));
    chunks[i++] = new Chunk(Vector(chunkCounter++, 0.0, 0.0));
    chunks[i++] = new Chunk(Vector(chunkCounter++, 0.0, 0.0));
    chunks[i++] = new Chunk(Vector(chunkCounter++, 0.0, 0.0));
  }
}

void Game::update(Chunk* chunk, const Vector playerLocation) {
  //chunk->processEntities();
  //std::cerr << (" process" + std::to_string(++m_process));
  float chunkDistance = 0;
  //float chunkDistance = Vector::getDistance(chunk->location, playerLocation);
  //std::cerr << ("distance" + std::to_string(++m_distance));
  if (chunkDistance > CHUNK_COUNT) {
    std::cerr << ("replace" + std::to_string(++m_replace));
    new (chunk) Chunk(Vector(chunkCounter++, 0.0, 0.0));
  }
}

void Game::updateChunks() {
  for (int i = 0; i < CHUNK_COUNT; i++) {
    m_threadPool.enqueue(std::bind(&Game::update, this, chunks[i], playerLocation));
  }
  //std::cerr << std::endl;
  m_threadPool.waitFinished();
}

int main(int argc, char *argv[]) {
  Game game;
  chrono::high_resolution_clock::time_point start;
  chrono::high_resolution_clock::time_point end;

  printf("loading world...\n");
  start = chrono::high_resolution_clock::now();
  game.loadWorld();
  end = chrono::high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::milliseconds>(
          end - start).count();
  printf("load time:%lu\n", duration);

  //spin
  int i = 0;
  double totTime = 0;
  while (1) {
    start = chrono::high_resolution_clock::now();
    Vector playerMovement = Vector(0.1, 0.0, 0.0);

    game.playerLocation = Vector::add(playerMovement, game.playerLocation);
    game.updateChunks();

    end = chrono::high_resolution_clock::now();

    double duration = chrono::duration_cast<chrono::nanoseconds>(end - start).count() /1000000.0;
    totTime += duration;

    if ((++i) % 1000 == 0)
      printf("%d - %f\n", i, totTime / i);

  }
}
