﻿using aerox.Runtime.Graphics;
using TerraFX.Interop.Vulkan;

namespace aerox.Runtime.Scene.Graphics.Drawers.Deferred;

public partial struct GBuffer
{
    public DeviceImage Color;
    public DeviceImage Location;
    public DeviceImage Normal;
    public DeviceImage RoughnessMetallicSpecular;
    public DeviceImage Emissive;

    public void Dispose()
    {
        Color.Dispose();
        Location.Dispose();
        Normal.Dispose();
        RoughnessMetallicSpecular.Dispose();
        Emissive.Dispose();
    }

    public void ImageBarrier(VkCommandBuffer cmd,VkImageLayout from,VkImageLayout to,ImageBarrierOptions? options = null)
    {
        GraphicsModule.ImageBarrier(cmd,Color,from,to,options);
        GraphicsModule.ImageBarrier(cmd,Location,from,to,options);
        GraphicsModule.ImageBarrier(cmd,Normal,from,to,options);
        GraphicsModule.ImageBarrier(cmd,RoughnessMetallicSpecular,from,to,options);
        GraphicsModule.ImageBarrier(cmd,Emissive,from,to,options);
    }
}