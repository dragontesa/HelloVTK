Disabling sunsky lights
/////////////////////////////////////////
To disable the sun and light from a SunSky actor in Unreal Engine, you can either adjust the intensity of the Directional Light component within the SunSky actor or disable the SunSky actor entirely. Alternatively, you can adjust the atmospheric settings or use a Post Process volume for exposure control. 
Methods to Disable/Control Sun and Light:

    1. Adjust Directional Light Intensity:
        Select the SunSky actor in your level.
        Locate the Directional Light component within the SunSky actor's details panel.
        Adjust the Intensity value to 0 or a very low value to effectively disable the sun's light. 

Alternatively, you can adjust the Source Angle of the Directional Light to change the sun's apparent size. 

2. Disable SunSky Actor:

    Select the SunSky actor in your level.
    In the Details panel, locate the Actor section.
    Uncheck the Enabled checkbox to completely disable the SunSky actor and its effects. 

3. Adjust Atmospheric Settings:

    Select the SunSky actor.
    In the Details panel, navigate to the Atmosphere section.
    Adjust parameters like Density Multiplier, Height Fog Contribution, or Absorption Multiplier to influence the overall atmospheric lighting. 

4. Use a Post Process Volume:

    Create a Post Process volume in your level and make it cover the area where you want to control the lighting.
    In the Post Process volume's details panel, navigate to the Exposure settings.
    Adjust parameters like Manual Exposure EV100, Min Brightness, and Max Brightness to control the exposure and overall brightness of the scene, effectively overriding the SunSky's lighting. 

Additional Tips:

    When working with SunSky, it's often recommended to remove any existing Directional Light, Sky Light, or Sky Atmosphere components to avoid conflicts. 

If you are experiencing overexposure, consider enabling "Extend default luminance range in Auto Exposure settings" in the Project Settings. 
You can also adjust the position of the sun using keyboard shortcuts like Ctrl + L and mouse movement. 
For more realistic sun and sky behavior, explore the Sky Atmosphere component's settings and features. 


Increase Ambient Light
/////////////////////////////////////////
To increase ambient light in Unreal Engine, you can utilize a Skylight, an Ambient Cubemap, or adjust settings within a post-process volume. Specifically, you can add a Skylight, adjust the intensity of an ambient cubemap, or enable experimental GI features and adjust settings in a post-process volume. 
Here's a more detailed breakdown:
1. Skylight:

    Navigate to Modes > Lights > Skylight.
    Place the Skylight in your scene.
    Adjust its properties, such as intensity and color, to achieve the desired ambient light level. 

2. Ambient Cubemap:

    Create a post-process volume (Go to Modes > Volumes > Post Process Volume).
    Make it unbound (check "Unbound") to affect the entire scene.
    Add an ambient cubemap to the post-process volume.
    Adjust the intensity of the cubemap to control the ambient light. 

3. Post-Process Volume and Auto Exposure:

    If you're experiencing issues with auto-exposure making the scene too dark or bright, you can disable it or adjust its settings.
    To disable auto-exposure, navigate to Edit - Project Settings, search for "auto exposure", and untick it.
    Then, create a post-process volume, set it to unbound, search for "auto exposure" within the volume settings, and set the metering method to manual with desired min/max brightness values. 

4. Directional Light:

    Add a directional light.
    Set its intensity to a low value.
    Disable shadows for the directional light.
    Adjust the color to a bluish tint to simulate ambient light. 

5. Other Considerations:

    Natural Lighting:
    Consider how natural light enters the scene (windows, doors, etc.) and how it can be used to enhance ambient lighting.
    Reflections:
    Use mirrors or reflective surfaces to bounce and distribute light within the scene.
    Materials and Color:
    Experiment with the color and materials of your scene to affect how light interacts with it. 
