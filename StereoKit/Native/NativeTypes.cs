﻿using System;
using System.Runtime.InteropServices;

namespace StereoKit
{
    /// <summary>Specifies details about how StereoKit should start up!</summary>
    public enum Runtime
    {
        /// <summary>Creates a flat, Win32 window, and simulates some MR functionality. Great for debugging.</summary>
        Flatscreen   = 0,
        /// <summary>Creates an OpenXR instance, and drives display/input through that.</summary>
        MixedReality = 1
    }

    /// <summary>StereoKit miscellaneous initialization settings! Setup StereoKit.settings with
    /// your data before calling StereoKitApp.Initialize.</summary>
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Settings
    {
        /// <summary>If using Runtime.Flatscreen, the pixel position of the window on the screen.</summary>
        public int flatscreenPosX;
        /// <summary>If using Runtime.Flatscreen, the pixel position of the window on the screen.</summary>
        public int flatscreenPosY;
        /// <summary>If using Runtime.Flatscreen, the pixel size of the window on the screen.</summary>
        public int flatscreenWidth;
        /// <summary>If using Runtime.Flatscreen, the pixel size of the window on the screen.</summary>
        public int flatscreenHeight;
        /// <summary>Where to look for assets when loading files! Final path will look like
        /// '[assetsFolder]/[file]', so a trailing '/' is unnecessary.</summary>
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string assetsFolder;
    }

    /// <summary>This describes the type of display tech used on a Mixed Reality device.</summary>
    public enum Display
    {
        /// <summary>This display is opaque, with no view into the real world! This is equivalent
        /// to a VR headset, or a PC screen.</summary>
        Opaque = 0,
        /// <summary>This display is transparent, and adds light on top of the real world. This is 
        /// equivalent to a HoloLens type of device.</summary>
        Additive,
        /// <summary>This is a physically opaque display, but with a camera passthrough displaying
        /// the world behind it anyhow. This would be like a Varjo XR-1, or phone-camera based AR.</summary>
        Passthrough,
    }

    /// <summary>Information about a system's capabilities and properties!</summary>
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct SystemInfo
    {
        /// <summary>The type of display this device has.</summary>
        public Display displayType;
    }

    /// <summary>Visual properties and spacing of the UI system.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct UISettings
    {
        /// <summary>Spacing between an item and its parent, in meters.</summary>
        public float padding;
        /// <summary>Spacing between sibling items, in meters.</summary>
        public float gutter;
        /// <summary>The Z depth of 3D UI elements, in meters.</summary>
        public float depth;
        /// <summary>How far up does the white back-border go on UI elements? This is a 0-1 percentage of the depth value.</summary>
        public float backplateDepth;
        /// <summary>How wide is the back-border around the UI elements? In meters.</summary>
        public float backplateBorder;
    }

    /// <summary>Textures come in various types and flavors! These are bit-flags
    /// that tell StereoKit what type of texture we want, and how the application
    /// might use it!</summary>
    [Flags]
    public enum TexType
    {
        /// <summary>A standard color image, without any generated mip-maps.</summary>
        ImageNomips  = 1 << 0,
        /// <summary>A size sided texture that's used for things like skyboxes, environment
        /// maps, and reflection probes. It behaves like a texture array with 6 textures.</summary>
        Cubemap      = 1 << 1,
        /// <summary>This texture can be rendered to! This is great for textures that might be
        /// passed in as a target to Renderer.Blit, or other such situations.</summary>
        Rendertarget = 1 << 2,
        /// <summary>This texture contains depth data, not color data!</summary>
        Depth        = 1 << 3,
        /// <summary>This texture will generate mip-maps any time the contents change. Mip-maps
        /// are a list of textures that are each half the size of the one before them! This is 
        /// used to prevent textures from 'sparkling' or aliasing in the distance.</summary>
        Mips         = 1 << 4,
        /// <summary>This texture's data will be updated frequently from the CPU (not renders)!
        /// This ensures the graphics card stores it someplace where writes are easy to do quickly.</summary>
        Dynamic      = 1 << 5,
        /// <summary>A standard color image that also generates mip-maps automatically.</summary>
        Image = ImageNomips | Mips,
    }

    /// <summary>What type of color information will the texture contain? A good default
    /// here is Rgba32.</summary>
    public enum TexFormat
    {
        /// <summary>Red/Green/Blue/Transparency data channels, at 8 bits per-channel. This is what you'll
        /// want most of the time. Matches well with the Color32 struct!</summary>
        Rgba32 = 0,
        /// <summary>Red/Green/Blue/Transparency data channels, at 16 bits per-channel! This is not
        /// common, but you might encounter it with raw photos, or HDR images.</summary>
        Rgba64,
        /// <summary>Red/Green/Blue/Transparency data channels at 32 bits per-channel! Basically 4 floats
        /// per color, which is bonkers expensive. Don't use this unless you know -exactly- what you're doing.</summary>
        Rgba128,
        /// <summary>A depth data format, 24 bits for depth data, and 8 bits to store stencil information!
        /// Stencil data can be used for things like clipping effects, deferred rendering, or shadow effects.</summary>
        DepthStencil,
        /// <summary>32 bits of data per depth value! This is pretty detailed, and is excellent for experiences
        /// that have a very far view distance.</summary>
        Depth32,
        /// <summary>16 bits of depth is not a lot, but it can be enough if your far clipping plane is pretty close.
        /// If you're seeing lots of flickering where two objects overlap, you either need to bring your far clip
        /// in, or switch to 32/24 bit depth.</summary>
        Depth16,
    }

    /// <summary>How does the shader grab pixels from the texture? Or more specifically,
    /// how does the shader grab colors between the provided pixels? If you'd like an
    /// in-depth explanation of these topics, check out [this exploration of texture filtering](https://medium.com/@bgolus/sharper-mipmapping-using-shader-based-supersampling-ed7aadb47bec)
    /// by graphics wizard Ben Golus.</summary>
    public enum TexSample
    {
        /// <summary>Use a linear blend between adjacent pixels, this creates a smooth,
        /// blurry look when texture resolution is too low.</summary>
        Linear = 0,
        /// <summary>Choose the nearest pixel's color! This makes your texture look like
        /// pixel art if you're too close.</summary>
        Point,
        /// <summary>This helps reduce texture blurriness when a surface is viewed at an
        /// extreme angle!</summary>
        Anisotropic
    }

    /// <summary>What happens when the shader asks for a texture coordinate that's outside
    /// the texture?? Believe it or not, this happens plenty often!</summary>
    public enum TexAddress
    {
        /// <summary>Wrap the UV coordinate around to the other side of the texture! This
        /// is basically like a looping texture, and is an excellent default. If you can 
        /// see weird bits of color at the edges of your texture, this may be due to Wrap
        /// blending the color with the other side of the texture, Clamp may be better in
        /// such cases.</summary>
        Wrap = 0,
        /// <summary>Clamp the UV coordinates to the edge of the texture! This'll create
        /// color streaks that continue to forever. This is actually really great for 
        /// non-looping textures that you know will always be accessed on the 0-1 range.</summary>
        Clamp,
        /// <summary>Like Wrap, but it reflects the image each time! Who needs this? I'm not sure!!
        /// But the graphics card can do it, so now you can too!</summary>
        Mirror,
    }

    /// <summary>Also known as 'alpha' for those in the know. But there's actually more than
    /// one type of transparency in rendering! The horrors. We're keepin' it fairly simple for
    /// now, so you get three options!</summary>
    public enum Transparency
    {
        /// <summary>Not actually transparent! This is opaque! Solid! It's the default option, and
        /// it's the fastest option! Opaque objects write to the z-buffer, the occlude pixels behind
        /// them, and they can be used as input to important Mixed Reality features like Late 
        /// Stage Reprojection that'll make your view more stable!</summary>
        None = 1,
        /// <summary>This will blend with the pixels behind it. This is transparent! It doesn't write
        /// to the z-buffer, and it's slower than opaque materials.</summary>
        Blend,
        /// <summary>This is sort of transparent! It can sample a texture, and discard pixels that are
        /// below a certain threshold. It doesn't blend with colors behind it, but it's pretty fast, and
        /// can write to the z-buffer no problem!</summary>
        Clip,
    }

    /// <summary>Culling is discarding an object from the render pipeline! This enum describes how mesh
    /// faces get discarded on the graphics card. With culling set to none, you can double the number of 
    /// pixels the GPU ends up drawing, which can have a big impact on performance. None can be appropriate
    /// in cases where the mesh is designed to be 'double sided'. Front can also be helpful when you want
    /// to flip a mesh 'inside-out'!</summary>
    public enum Cull
    {
        /// <summary>Discard if the back of the triangle face is pointing towards the camera.</summary>
        Back = 0,
        /// <summary>Discard if the front of the triangle face is pointing towards the camera.</summary>
        Front,
        /// <summary>No culling at all! Draw the triangle regardless of which way it's pointing.</summary>
        None,
    }

    /// <summary>What type of data does this material parameter need? This is used to tell the 
    /// shader how large the data is, and where to attach it to on the shader.</summary>
    public enum MaterialParam
    {
        /// <summary>A single 32 bit float value.</summary>
        Float = 0,
        /// <summary>A color value described by 4 floating point values.</summary>
        Color128,
        /// <summary>A 4 component vector composed of loating point values.</summary>
        Vector,
        /// <summary>A 4x4 matrix of floats.</summary>
        Matrix,
        /// <summary>Texture information!</summary>
        Texture,
    }

    /// <summary>A bit-flag enum for describing alignment or positioning. Items can be
    /// combined using the '|' operator, like so:
    /// 
    /// `TextAlign alignment = TextAlign.XLeft | TextAlign.YTop;`
    /// 
    /// Avoid combining multiple items of the same axis, and note that a few items, 
    /// like `Center` are already a combination of multiple flags.</summary>
    [Flags]
    public enum TextAlign
    {
        /// <summary>On the x axis, this item should start on the left.</summary>
        XLeft   = 1 << 0,
        /// <summary>On the y axis, this item should start at the top.</summary>
        YTop    = 1 << 1,
        /// <summary>On the x axis, the item should be centered.</summary>
        XCenter = 1 << 2,
        /// <summary>On the y axis, the item should be centered.</summary>
        YCenter = 1 << 3,
        /// <summary>On the x axis, this item should start on the right.</summary>
        XRight  = 1 << 4,
        /// <summary>On the y axis, this item should start on the bottom.</summary>
        YBottom = 1 << 5,
        /// <summary>A combination of XCenter and YCenter.</summary>
        Center  = XCenter | YCenter,
    }

    /// <summary>A text style is a font plus size/color/material parameters, and are 
    /// used to keep text looking more consistent through the application by encouraging 
    /// devs to re-use styles throughout the project. See Text.MakeStyle for making a 
    /// TextStyle object.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct TextStyle
    {
        internal int id;
    }

    /// <summary>This describes the behavior of a 'Solid' physics object! the physics 
    /// engine will apply forces differently </summary>
    public enum SolidType
    {
        /// <summary>This object behaves like a normal physical object, it'll fall, get
        /// pushed around, and generally be succeptible to physical forces! This is a 
        /// 'Dynamic' body in physics simulation terms.</summary>
        Normal = 0,
        /// <summary>Immovable objects are always stationary! They have infinite mass,
        /// zero velocity, and can't collide with Immovable of Unaffected types.</summary>
        Immovable,
        /// <summary>Unaffected objects have infinite mass, but can have a velocity! They'll
        /// move under their own forces, but nothing in the simulation will affect them. 
        /// They don't collide with Immovable or Unaffected types.</summary>
        Unaffected,
    }

    /// <summary>The way the Sprite is stored on the backend! Does it get batched and atlased 
    /// for draw efficiency, or is it a single image?</summary>
    public enum SpriteType
    {
        /// <summary>The sprite will be batched onto an atlas texture so all sprites can be 
        /// drawn in a single pass. This is excellent for performance! The only thing to 
        /// watch out for here, adding a sprite to an atlas will rebuild the atlas texture!
        /// This can be a bit expensive, so it's recommended to add all sprites to an atlas
        /// at start, rather than during runtime. Also, if an image is too large, it may take
        /// up too much space on the atlas, and may be better as a Single sprite type.</summary>
        Atlased = 0,
        /// <summary>This sprite is on its own texture. This is best for large images, items
        /// that get loaded and unloaded during runtime, or for sprites that may have edge
        /// artifacts or severe 'bleed' from adjacent atlased images.</summary>
        Single
    }

    /// <summary>What type of device is the source of the pointer? This is a bit-flag that can 
    /// contain some input source family information.</summary>
    [Flags]
    public enum InputSource
    {
        /// <summary>Matches with all input sources!</summary>
        Any        = 0x7FFFFFFF,
        /// <summary>Matches with any hand input source.</summary>
        Hand       = 1 << 0,
        /// <summary>Matches with left hand input sources.</summary>
        HandLeft   = 1 << 1,
        /// <summary>Matches with right hand input sources.</summary>
        HandRight  = 1 << 2,
        /// <summary>Matches with Gaze category input sources.</summary>
        Gaze       = 1 << 4,
        /// <summary>Matches with the head gaze input source.</summary>
        GazeHead   = 1 << 5,
        /// <summary>Matches with the eye gaze input source.</summary>
        GazeEyes   = 1 << 6,
        /// <summary>Matches with mouse cursor simulated gaze as an input source.</summary>
        GazeCursor = 1 << 7,
        /// <summary>Matches with any input source that has an activation button!</summary>
        CanPress   = 1 << 8,
    }

    [Flags]
    public enum PointerState
    {
        None        = 0,
        Available   = 1 << 0,
    }

    /// <summary>A bit-flag that tracks the state of an input!</summary>
    [Flags]
    public enum InputState
    {
        /// <summary>A default none state.</summary>
        None        = 0,
        /// <summary>Matches with any input state!</summary>
        Any         = 0x7FFFFFFF,
        /// <summary>Matches with input states that are currently tracked.</summary>
        Tracked     = 1 << 0,
        /// <summary>Matches with input states that have just become tracked! This 
        /// is only true for a single frame at a time.</summary>
        JustTracked = 1 << 1,
        /// <summary>Matches with input states that have just stopped being tracked.
        /// is only true for a single frame at a time.</summary>
        Untracked   = 1 << 2,
        Pinch       = 1 << 3,
        JustPinch   = 1 << 4,
        Unpinch     = 1 << 5,
        Grip        = 1 << 6,
        JustGrip    = 1 << 7,
        Ungrip      = 1 << 8,
    }

    /// <summary>A bit-flag for the current state of a button input.</summary>
    [Flags]
    public enum BtnState
    {
        /// <summary>Is the button currently up, unpressed?</summary>
        Up       = 0,
        /// <summary>Is the button currently down, pressed?</summary>
        Down     = 1 << 0,
        /// <summary>Has the button just been released? Only true for a single frame.</summary>
        JustUp   = 1 << 1,
        /// <summary>Has the button just been pressed? Only true for a single frame.</summary>
        JustDown = 1 << 2,
        /// <summary>Has the button just changed state this frame?</summary>
        Changed  = JustUp | JustDown,
    }

    /// <summary>A collection of extension methods for the BtnState enum that makes
    /// bit-field checks a little easier.</summary>
    public static class BtnStateExtensions
    {
        /// <summary>Is the button pressed?</summary>
        /// <returns>True if pressed, false if not.</returns>
        public static bool IsPressed     (this BtnState state) => (state & BtnState.Down    ) > 0;
        /// <summary>Has the button just been pressed this frame?</summary>
        /// <returns>True if pressed, false if not.</returns>
        public static bool IsJustPressed (this BtnState state) => (state & BtnState.JustDown) > 0;
        /// <summary>Has the button just been released this frame?</summary>
        /// <returns>True if released, false if not.</returns>
        public static bool IsJustReleased(this BtnState state) => (state & BtnState.JustUp  ) > 0;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void InputEventCallback(InputSource source, InputState type, IntPtr pointer);

    [StructLayout(LayoutKind.Sequential)]
    public struct Pointer
    {
        public InputSource  source;
        public PointerState state;
        public Ray          ray;
        public Quat         orientation;
    }

    /// <summary>This stores information about the mouse! What's its state, where's
    /// it pointed, do we even have one?</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Mouse
    {
        /// <summary>Is the mouse available to use? Most MR systems likely won't have
        /// a mouse!</summary>
        public bool  available;
        /// <summary>Position of the mouse relative to the window it's in! This is the number
        /// of pixels from the top left corner of the screen.</summary>
        public Vec2  pos;
        /// <summary>How much has the mouse's position changed in the current frame? Measured 
        /// in pixels.</summary>
        public Vec2  posChange;
        /// <summary>What's the current scroll value for the mouse's scroll wheel? TODO: Units</summary>
        public float scroll;
        /// <summary>How much has the scroll wheel value changed during this frame? TODO: Units</summary>
        public float scrollChange;
    }

    public enum Key
    {
        MouseLeft = 0x01, MouseRight = 0x02, MouseCenter = 0x04,
        Backspace = 0x08, Tab = 0x09,
        Return = 0x0D, Shift = 0x10,
        Ctrl = 0x11, Alt = 0x12,
        Esc = 0x1B, Space = 0x20,
        End = 0x23, Home = 0x24,
        Left = 0x25, Right = 0x27,
        Up = 0x26, Down = 0x28,
        Printscreen = 0x2A, Insert = 0x2D, Del = 0x2E,
        N0 = 0x30, N1 = 0x31, N2 = 0x32, N3 = 0x33, N4 = 0x34, N5 = 0x35, N6 = 0x36, N7 = 0x37, N8 = 0x38, N9 = 0x39,
        A = 0x41, B = 0x42, C = 0x43, D = 0x44, E = 0x45, F = 0x46, G = 0x47, H = 0x48, I = 0x49, J = 0x4A, K = 0x4B, L = 0x4C, M = 0x4D, N = 0x4E, O = 0x4F, P = 0x50, Q = 0x51, R = 0x52, S = 0x53, T = 0x54, U = 0x55, V = 0x56, W = 0x57, X = 0x58, Y = 0x59, Z = 0x5A,
        LCmd = 0x5B, RCmd = 0x5C,
        Num0 = 0x60, Num1 = 0x61, Num2 = 0x62, Num3 = 0x63, Num4 = 0x64, Num5 = 0x65, Num6 = 0x66, Num7 = 0x67, Num8 = 0x68, Num9 = 0x69,
        Multiply = 0x6A, Add = 0x6B, Subtract = 0x6D, Decimal = 0x6E, Divide = 0x6F,
        F1 = 0x70, F2 = 0x71, F3 = 0x72, F4 = 0x73, F5 = 0x74, F6 = 0x75, F7 = 0x76, F8 = 0x77, F9 = 0x78, F10 = 0x79, F11 = 0x7A, F12 = 0x7B,
        MAX = 0xFF,
    };

    /// <summary>An enum for indicating which hand to use!</summary>
    public enum Handed
    {
        /// <summary>Left hand.</summary>
        Left = 0,
        /// <summary>Right hand.</summary>
        Right,
        /// <summary>The number of hands one generally has, this is much nicer than doing
        /// a for loop with '2' as the condition! It's much clearer when you can loop Hand.Max
        /// times instead.</summary>
        Max,
    }

    /// <summary>Severity of a log item.</summary>
    public enum LogLevel
    {
        /// <summary>This is for diagnostic information, where you need to know details about what -exactly-
        /// is going on in the system. This info doesn't surface by default.</summary>
        Diagnostic = 0,
        /// <summary>This is non-critical information, just to let you know what's going on.</summary>
        Info,
        /// <summary>Something bad has happened, but it's still within the realm of what's expected.</summary>
        Warning,
        /// <summary>Danger Will Robinson! Something really bad just happened and needs fixing!</summary>
        Error
    }

    /// <summary>Index values for each finger! From 0-4, from thumb to little finger.</summary>
    public enum Finger
    {
        /// <summary>Finger 0.</summary>
        Thumb  = 0,
        /// <summary>The primary index/pointer finger! Finger 1.</summary>
        Index  = 1,
        /// <summary>Finger 2, next to the index finger.</summary>
        Middle = 2,
        /// <summary>Finger 3! What does one do with this finger? I guess... wear
        /// rings on it?</summary>
        Ring   = 3,
        /// <summary>Finger 4, the smallest little finger! AKA, The Pinky.</summary>
        Little = 4
    }

    /// <summary>Here's where hands get crazy! Technical terms, and watch out for
    /// the thumbs!</summary>
    public enum FingerJoint
    {
        /// <summary>Joint 0. This is at the base of the hand, right above the wrist. For 
        /// the thumb, Metacarpal and Proximal have the same value.</summary>
        Metacarpal   = 0,
        /// <summary>Joint 1. These are the knuckles at the top of the palm! For 
        /// the thumb, Metacarpal and Proximal have the same value.</summary>
        Proximal     = 1,
        /// <summary>Joint 2. These are the knuckles in the middle of the finger! First
        /// joints on the fingers themselves.</summary>
        Intermediate = 2,
        /// <summary>Joint 3. The joints right below the fingertip!</summary>
        Distal       = 3,
        /// <summary>Joint 4. The end/tip of each finger!</summary>
        Tip          = 4
    }
}