﻿using System.Runtime.InteropServices;

namespace aerox.Runtime.Widgets.Draw.Commands;

public class ClipRect : Clip
{
    [StructLayout(LayoutKind.Sequential)]
    struct PushConstant
    {
        
    }

    public override void Run(WidgetFrame frame)
    {
        throw new NotImplementedException();
    }
}