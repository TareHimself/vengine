﻿using aerox.Runtime.Math;
using aerox.Runtime.Scene.Graphics;

namespace aerox.Runtime.Scene.Components;

public class RenderedComponent : SceneComponent, ISceneDrawable
{
    public virtual void Draw(SceneFrame frame, Matrix4 parentSpace)
    {
        
    }
}