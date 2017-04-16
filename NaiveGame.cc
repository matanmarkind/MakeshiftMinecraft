// clarity.cpp : Defines the entry point for the console application.
//
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <thread>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;
using namespace std::chrono;

class Vector
{
public:
  float x,y,z;
  Vector();
  Vector(float,float,float);

  static Vector add(Vector a,Vector b);
  static Vector mul(Vector a,Vector b);
  static Vector sub(Vector a,Vector b);
  static float getDistance(Vector a, Vector b);
};

Vector::Vector()
{
  x = 0;
  y = 0;
  z = 0;
}


Vector::Vector (float x,float y,float z)
{
  this->x = x;
  this->y = y;
  this->z = z;
}

float Vector::getDistance(Vector a, Vector b)
{
  Vector s = Vector::sub(a,b);
  float result = (float)sqrt(s.x*s.x+s.y*s.y+s.z*s.z);
  return result;
}

Vector Vector::add(Vector a,Vector b)
{
  Vector v = Vector(a.x+b.x,a.y+b.y,a.z+b.z);
  return v;
}

Vector Vector::mul(Vector a,Vector b)
{
  Vector v = Vector(a.x*b.x,a.y*b.y,a.z*b.z);
  return v;
}

Vector Vector::sub(Vector a,Vector b)
{
  Vector v = Vector(a.x-b.x,a.y-b.y,a.z-b.z);
  return v;
}

class Block
{
public:
  Vector location;
  string name;
  int durability;
  int textureid;
  bool breakable;
  bool visible;
  int type;

  Block(Vector,string,int,int,bool,bool,int);
  ~Block();

};

Block::Block(Vector location,string name,int durability,int textureid,bool breakable,bool visible,int type)
{
  this->location = location;
  this->name = name;
  this->textureid = textureid;
  this->breakable = breakable;
  this->visible = visible;
  this->type = type;
}

Block::~Block()
{

}

class Entity
{
public:
  Vector location;
  string name;
  int health;
  Vector speed;
  enum class Type { Zombie,Chicken,Exploder,TallCreepyThing };
  Entity(Vector ,Type);
  ~Entity();
  void updatePosition();

};

Entity::Entity (Vector location,Type type)
{
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

Entity::~Entity()
{

}

void Entity::updatePosition()
{


  Vector rndUnitIshVector = Vector(1,1,1);

  Vector movementVector = Vector::mul(rndUnitIshVector,speed);

  location = movementVector;



}


class Chunk
{
  static const int NUM_BLOCKS = 65536;
  static const int NUM_ENTITIES = 1000;



public:
  vector<Block*> blocks;
  vector<Entity*> entities;
  Vector location;
  Chunk(Vector);
  ~Chunk();
  void processEntities();

};


Chunk::Chunk(Vector location)
{
  this->location = location;
  blocks = vector<Block*>(NUM_BLOCKS);
  for (int i = 0; i < NUM_BLOCKS;i++)
  {
    Block* b = new Block(Vector(i,i,i),"Block" + std::to_string(i),100,1,true,true,1);
    blocks[i] = b;
  }

  entities = vector<Entity*>(NUM_ENTITIES);
  for (int i = 0; i < NUM_ENTITIES;i=i+4)
  {
    entities[i] = new Entity(Vector(i,i,i),Entity::Type::Chicken);
    entities[i+1] = new Entity(Vector(i+1,i,i),Entity::Type::Zombie);
    entities[i+2] = new Entity(Vector(i+2,i,i),Entity::Type::Exploder);
    entities[i+3] = new Entity(Vector(i+3,i,i),Entity::Type::TallCreepyThing);
  }

}

Chunk::~Chunk()
{

  for (auto block:blocks)
  {
    delete block;
  }

  for (auto entity:entities)
  {
    delete entity;
  }


}

void Chunk::processEntities()
{

  for(auto entity:entities)
  {

    entity->updatePosition();
  }
}


class Game
{
public:

  static const int CHUNK_COUNT = 100;
  vector<Chunk*> chunks;
  Vector playerLocation;
  int chunkCounter;
  Game();
  void loadWorld();
  void updateChunks();

};

Game::Game()
{

  chunkCounter = 0;
  playerLocation = Vector(0,0,0);
}

void Game::loadWorld() {
  chunks = vector<Chunk*>(CHUNK_COUNT);
  for (int i = 0; i < CHUNK_COUNT;i++)
  {
    chunks[i] = new Chunk(Vector(chunkCounter,0.0,0.0));
    chunkCounter++;
  }


}

void Game::updateChunks()
{

  vector<Chunk*> toRemove = vector<Chunk*>();
  for (auto chunk:chunks)
  {

    chunk->processEntities();

    float chunkDistance = Vector::getDistance(chunk->location,playerLocation);
    if (chunkDistance > CHUNK_COUNT)
    {
      toRemove.push_back(chunk);
    }
  }

  for (auto chunk:toRemove)
  {
    // srsly?
    vector<Chunk*>::iterator pos = find(chunks.begin(),chunks.end(),chunk);
    chunks.erase(pos);
    chunks.push_back(new Chunk(Vector(chunkCounter,0.0,0.0)));
    chunkCounter++;
    delete chunk;
  }

}

int main()
{
  Game game = Game();
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
  double totTime = 0;
  while(1)
  {

    start = high_resolution_clock::now();
    Vector playerMovement = Vector(0.1,0.0,0.0);

    game.playerLocation = Vector::add(playerMovement,game.playerLocation);


    game.updateChunks();

    end = high_resolution_clock::now();

    auto duration = (double)(duration_cast<nanoseconds>(end-start).count() / 1000000.0);
    totTime += duration;

    if((++i)%1000 == 0)
      printf("%d - %f\n",i, totTime/i);




  }


}