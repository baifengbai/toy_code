% default agent is 2D particle
classdef BasicAgent
   properties
      px = 0;
      py = 0;
      vx = 0;
      vy = 0;
      ux = 0;
      uy = 0;
   end
   methods
      % constructor
      function obj = BasicAgent(pos,vel)
          obj.px = pos(1);
          obj.py = pos(2);
          obj.vx = vel(1);
          obj.vy = vel(2);
          obj.ux = 0;
          obj.uy = 0;
      end
   
      function obj = sim(obj, dt)

          obj.px = obj.px + obj.vx*dt;
          obj.py = obj.py + obj.vy*dt;
          obj.vx = obj.vx + obj.ux*dt;
          obj.vy = obj.vy + obj.uy*dt;
      end
      
      function obj = setPos(obj, pos)
          obj.px = pos(1);
          obj.py = pos(2);
      end
      
      function obj = setVel(obj, vel)
          obj.vx = vel(1);
          obj.vy = vel(2);
      end
      
      function obj = setCtrl(obj, u)
          obj.ux = u(1);
          obj.uy = u(2);
      end
   end
end