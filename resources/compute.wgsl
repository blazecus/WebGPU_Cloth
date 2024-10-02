struct Particle {
  pos : vec3<f32>,
  vel : vec3<f32>,
};

struct Vertex {
  pos : vec3<f32>,
  norm : vec3<f32>,
};

struct SimParams {
  particleWidth : f32,
  particleHeight : f32,
  particleDist : f32,
  particleMass : f32,
  particleScale : f32,

  outSpringStretch : f32,
  inSpringStretch : f32,

  sphereRadius : f32,
  sphereX : f32,
  sphereY : f32,
  sphereZ : f32,

  deltaT : f32,
  currentT : f32,
}

@group(0) @binding(0) var<uniform> params : SimParams;
@group(0) @binding(1) var<storage, read> particlesSrc : array<Particle>;
@group(0) @binding(2) var<storage, read_write> particlesDst : array<Particle>;

@group(1) @binding(0) var<storage, read_write> vertexOut : array<Vertex>;

fn forces(index: u32, current_pos: vec3<f32>)->vec3<f32>{
  let width :i32= i32(params.particleWidth);
  
  let x = i32(index) % width;
  let y = i32(index) / width;

  var total_force = vec3<f32>();
  let rest_dist = params.particleDist * 0.95f;// * 2.0f/3.0f;

  let k1 = 73.0f / params.particleScale;
  let k2 = 12.5f / params.particleScale;
  // all 8 surrounding
  for (var addx:i32 = -1; addx < 2; addx++){
    for (var addy:i32 = -1; addy < 2; addy++){
      // apply short spring forces
      var diag_dist = 1.0f;
      if(abs(addx) + abs(addy) == 2){
        diag_dist = 1.41421356237f;
        //diag_dist = sqrt(2.0f);
      }
      let indx:i32 = x + addx;
      let indy:i32 = y + addy;
      var new_index: i32 = indx + indy * width;

      if(indx >= 0 && indx < width && indy >= 0 && indy < i32(params.particleHeight) && (addx != 0 || addy != 0)){
        let diff = current_pos - particlesSrc[new_index].pos;
        if(rest_dist * diag_dist < length(diff)){
          let spring_force = normalize(diff) * (rest_dist * diag_dist - length(diff)) * k1; 
          total_force = total_force + spring_force;
        }
      }

      // apply long spring forces
      let farx = indx + addx;
      let fary = indy + addy;
      var long_new_index: i32 = farx + fary * width;

      if(farx >= 0 && farx < width && fary >= 0 && fary < i32(params.particleHeight) && addx != 0 && addy != 0){
        let diff = current_pos - particlesSrc[long_new_index].pos;
        if(rest_dist * diag_dist * 2.0f > length(diff)){
          let spring_force = normalize(diff) * (rest_dist * diag_dist * 2- length(diff)) * k2; 
          total_force = total_force + spring_force;
        }
      }
    }
  }

  let sphere_pos = vec3(params.sphereX, params.sphereY, params.sphereZ);
  let sphere_dist = current_pos - sphere_pos;
  if(length(sphere_dist) < params.sphereRadius){
    let sphere_diff = params.sphereRadius - length(sphere_dist);
    total_force += normalize(sphere_dist) * sphere_diff * sphere_diff * 200.0f;
  }

  // gravity
  total_force.y -= 9.8 * params.particleMass; 

  // wind - add randomness based on 3d position, index, time
  let t = params.currentT;
  //let wind_dir = vec3(sin(cos(t * t * 5.0f)), cos(t * t * t + 5.0f), cos(sin(t))) * t;
  let wind_dir = vec3(sin(t * 0.25f), cos(t * 0.15f), sin(t * t * 0.015f));
  //total_force += wind_dir  * 0.5f;
  total_force.z += 0.0005f * params.particleScale;

  var multiplier = 1.0f;
  if(y == i32(params.particleHeight - 1)){
    multiplier = 0.0f;
  }

  // fix edge to pole
  //let distance_from_edge = min(min(u32(x), u32(width - 1 - x)), min(u32(y), u32(i32(params.particleHeight) - y - 1)));

  //var distance_from_edge = x;
  //let distance_from_edge = i32(params.particleHeight) - 1 - y;
  //let cutoff = max(i32(params.particleHeight / 10.0f), 1); 
  //let cutoff = 1;

  //if(distance_from_edge < cutoff){
  //  multiplier = f32(distance_from_edge) / f32(cutoff);
  //}
  //let height = i32(params.particleHeight);
  //let corner_cutoff_x = max(width / 10, 1);
  //let corner_cutoff_y = max(height / 10, 1);
  //if((x < corner_cutoff_x || x > width - 1 - corner_cutoff_x) && (y < corner_cutoff_y || y > height - 1 - corner_cutoff_y)){
  //  multiplier = 0.0f;
  //}

  return total_force * multiplier; 
}

fn triangle_pos_conversion(square_pos: u32) -> u32{
  var diff = 0;
  //get proper position index
  if(square_pos == 1 || square_pos == 3){
    diff = i32(params.particleWidth);
  }
  else if(square_pos == 2 || square_pos == 5){
    diff = 1;
  }
  else if(square_pos == 4){
    diff = i32(params.particleWidth) + 1;
  }
  
  return u32(diff);
}

@compute
@workgroup_size(64)
fn main(@builtin(global_invocation_id) global_invocation_id: vec3<u32>) {
  let total = arrayLength(&particlesSrc);
  let index = global_invocation_id.x;
  if (index >= total) {
    return;
  }

  var vPos : vec3<f32> = particlesSrc[index].pos;
  var vVel : vec3<f32> = particlesSrc[index].vel;

  //RK4 integration

  let dt = params.deltaT;

  let k0 = dt * vVel;
  let l0 = dt * forces(index, vPos);
  let k1 = dt * (vVel + l0 * 0.5f);
  let l1 = dt * forces(index, vPos + k0 * 0.5f);
  let k2 = dt * (vVel + l1 * 0.5f);
  let l2 = dt * forces(index, vPos + k1 * 0.5f);
  let k3 = dt * (vVel + l2);
  let l3 = dt * forces(index, vPos + k2);
  
  vPos = vPos + (k0 + 2.0f * k1 + 2.0f * k2 + k3) / 6.0f;
  vVel = vVel + (l0 + 2.0f * l1 + 2.0f * l2 + l3) / 6.0f;
  let width = i32(params.particleWidth);
  let height = i32(params.particleHeight);
  let iy:i32 = i32(index) / width;
  let ix:i32= i32(index) % width; 
  
  if(iy < height - 1){
  for (var addx:i32 = -1; addx < 2; addx++){
    for (var addy:i32 = -1; addy < 2; addy++){
      let indx:i32 = ix + addx;
      let indy:i32 = iy + addy;
      if(indx >= 0 && indx < width && indy >= 0 && indy < height && (addx != 0 || addy != 0)){
        let new_index : i32 = indx + indy * width;
        let diff = vPos - particlesSrc[new_index].pos;
        var diag_dist = 1.0f;
        if(abs(addx) + abs(addy) == 2){
          diag_dist = 1.41421356237f;
        }
        diag_dist *= params.particleDist;

        if(length(diff) < params.inSpringStretch * diag_dist){
          vPos = particlesSrc[new_index].pos + normalize(diff) * diag_dist * params.inSpringStretch;
        }
        else if(length(diff) > params.outSpringStretch * diag_dist){
          vPos = particlesSrc[new_index].pos + normalize(diff) * diag_dist * params.outSpringStretch;
        }
      }
    }
  }
  }
  // Write back
  particlesDst[index] = Particle(vPos, vVel);
}

@compute
@workgroup_size(64)
fn particle_to_vertex(@builtin(global_invocation_id) global_invocation_id: vec3<u32>) {
  let total = arrayLength(&vertexOut);
  let index = global_invocation_id.x;
  if (index >= total) {
    return;
  }

  let cell: u32 = index / 6;
  let row: u32 = cell / u32(i32(params.particleWidth-1));
  var cellIdx: u32 = cell + row;

  let square_pos: u32 = index % 6;
  var add_to_lr: u32 = 0;
  if(square_pos > 2){
    add_to_lr = u32(3);
  }
  let left: u32 = ((square_pos + 2) % 3) + add_to_lr;
  let right: u32 = ((square_pos + 1) % 3) + add_to_lr;

  let lcellIdx = cellIdx + triangle_pos_conversion(left);
  let rcellIdx = cellIdx + triangle_pos_conversion(right);
  cellIdx += triangle_pos_conversion(square_pos);

  let vpos :vec3<f32> = particlesDst[cellIdx].pos;
  let lvec :vec3<f32> = vpos - particlesDst[lcellIdx].pos;
  let rvec :vec3<f32> = vpos - particlesDst[rcellIdx].pos;

  let norm = normalize(cross(lvec, rvec));
  vertexOut[index] = Vertex(vpos / (0.1f * params.particleScale), norm);
}