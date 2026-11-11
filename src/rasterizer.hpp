#ifndef CS418_RASTERIZER_HPP
#define CS418_RASTERIZER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rast
{

constexpr std::size_t kValueSize = 8;
using Value = std::array<double, kValueSize>;
using Position = std::array<double, 4>;
using Color = std::array<double, 4>;

enum Component : std::size_t
{
  X = 0,
  Y = 1,
  Z = 2,
  InvW = 3,
  R = 4,
  G = 5,
  B = 6,
  A = 7,
};

std::vector<Value> DDA(Value first, Value second, std::size_t dimension);

class Rasterizer
{
 public:
  void ConfigureOutput(unsigned width, unsigned height, std::string filename);
  void SetPositions(unsigned size, const std::vector<double>& values);
  void SetColors(unsigned size, const std::vector<double>& values);
  void DrawArraysTriangles(std::size_t first, std::size_t count);
  void Save() const;

  unsigned Width() const { return width_; }
  unsigned Height() const { return height_; }
  const std::string& OutputFilename() const { return outputFilename_; }
  const std::vector<std::uint8_t>& Pixels() const { return pixels_; }

 private:
  struct PipelineState
  {
    // Reserved state keeps the core layout ready for later assignment electives.
    bool depth = false;
    bool srgb = false;
    bool hyperbolic = false;
    bool cull = false;
    bool frustum = false;
    unsigned fsaa = 1;
    std::vector<unsigned> elements;
    std::vector<std::array<double, 2>> texcoords;
    std::vector<double> pointSizes;
    std::array<double, 16> matrix{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };
  };

  Value ProcessVertex(std::size_t index) const;
  void DrawTriangle(Value first, Value second, Value third);
  void WritePixel(const Value& value);
  void RequireConfigured() const;

  unsigned width_ = 0;
  unsigned height_ = 0;
  std::string outputFilename_;
  std::vector<std::uint8_t> pixels_;
  std::vector<Position> positions_;
  std::vector<Color> colors_;
  PipelineState state_;
};

Rasterizer RenderFile(const std::string& inputFilename);

}  // namespace rast

#endif
