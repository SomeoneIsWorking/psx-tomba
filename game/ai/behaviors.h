#pragma once
class Core;

class Behaviors {
public:
  // FUN_80113C5C, A00 overlay. a0-free: reads the master G block directly.
  static void areaSeasidePerframe(Core *c);
};
