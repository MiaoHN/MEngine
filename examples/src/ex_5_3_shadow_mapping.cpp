// = LearnOpenGL 5.advanced_lighting/3.1.3.shadow_mapping
//   source: LearnOpenGL/src/5.advanced_lighting/3.1.3.shadow_mapping/shadow_mapping.cpp
//
// LO-exact directional shadow port on the "blinn_lo" path with the engine's
// directional shadow map applied to the LO directional light
// (Scene::SetLoDirShadow). Blinn-Phong halfway specular (LO 3.1.3.fs), wood
// everywhere.
//   - 50x50 wood floor (UV 0..25) + three wood cubes (LO positions/scales)
//   - a single dim directional "sun": ambient 0.09 / diffuse 0.3 / specular 0.3
//     (LO lightColor 0.3, ambient 0.3*0.3), Blinn shininess 64
//   - LO camera (0,0,3) FOV 45, 4:3 window, clear 0.1, raw linear output
#include <memory>

#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;
using MEngine::examples::PutAxis;

namespace {
std::shared_ptr<Scene> BuildShadowMapping() {
  auto s = std::make_shared<Scene>();

  // --- wood floor: LO 50x50 plane at y=-0.5, wood tiled 25x.
  Put(*s, examples::TiledPlane(50.0f, 25.0f), examples::BlinnLoDiffuse("textures/wood.png", 64.0f),
      {0.0f, -0.5f, 0.0f});

  // --- three wood cubes (LO positions; engine cube is unit sized so scale =
  //     2 x LO's (LO's renderCube is a +/-1 cube)).
  const auto wood = []() { return examples::BlinnLoDiffuse("textures/wood.png", 64.0f); };
  Put(*s, Mesh::CreateCube(), wood(), {0.0f, 1.5f, 0.0f}, 1.0f);              // LO scale .5
  Put(*s, Mesh::CreateCube(), wood(), {2.0f, 0.0f, 1.0f}, 1.0f);              // LO scale .5
  PutAxis(*s, Mesh::CreateCube(), wood(), {-1.0f, 0.0f, 2.0f},
          glm::normalize(glm::vec3(1, 0, 1)), 60.0f, 0.5f);                   // LO scale .25

  // --- the dim directional sun (LO: lightColor 0.3, ambient 0.3*0.3, Blinn
  //     shininess 64). LO's lightPos (-2,4,-1) shines toward the origin, so the
  //     travel direction is origin - lightPos = (2,-4,1).
  s->GetLight().direction = glm::normalize(glm::vec3(2.0f, -4.0f, 1.0f));
  s->GetLight().ambient   = glm::vec3(0.09f);
  s->GetLight().diffuse   = glm::vec3(0.3f);
  s->GetLight().specular  = glm::vec3(0.3f);

  // LO 3.1.3 uses a Blinn halfway specular AND a real directional shadow.
  s->SetLoBlinnSpec(true);
  s->SetLoDirShadow(true);

  // LO clears to 0.1 and writes the raw (untonemapped) result.
  examples::LoScene(*s, glm::vec3(0.1f, 0.1f, 0.1f));
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildShadowMapping, "LO 5.3 shadow_mapping", {0, 0, 0}, 0.0f, 0.0f,
                                           3.0f, 45.0f});
}
