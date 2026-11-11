#include "rasterizer.hpp"

#include <exception>
#include <iostream>

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "usage: " << argv[0] << " input.txt\n";
    return 2;
  }

  try
  {
    rast::RenderFile(argv[1]);
  }
  catch (const std::exception& error)
  {
    std::cerr << "rasterizer: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
