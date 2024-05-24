﻿using System.Runtime.InteropServices;

namespace aerox.Runtime.Math;

public static partial class Glm
{
    [LibraryImport(Dlls.AeroxNative, EntryPoint = "mathGlmOrthographic")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl) ])]
    private static partial void NativeGlmOrthographic(ref Matrix4 result,float left,float right,float bottom, float top);


    public static Matrix4 Orthographic(float left, float right, float bottom, float top)
    {
        var result = Matrix4.Identity;
        NativeGlmOrthographic(ref result,left,right,bottom,top);
        return result;
    }

}