// Copyright (C) Microsoft Corporation. All rights reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using VirtualCameraManager_WinRT;
using CameraKsPropertyHelper;
using Windows.Media.Devices;
using Windows.UI.Xaml.Controls;


namespace VirtualCameraManager_App.controls
{
    /// <summary>
    /// Class that represent the group of UI interative elements to toggle video device controls.
    /// </summary>
    public sealed partial class VideoDeviceControls : UserControl
    {
        // members
        private VideoDeviceController m_controller = null;

        // methods
        public VideoDeviceControls(VideoDeviceController controller)
        {
            m_controller = controller;

            this.InitializeComponent();
        }

        /// <summary>
        /// Initialize the set of video device controls to be exposed in the UI.
        /// </summary>
        public void Initialize()
        {
            UIPanelRows.Children.Clear();

            var effect = new EffectToggle();
            var controlPayload = PropertyInquiry.GetExtendedControl(m_controller, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION);
            effect.Initialize(
                "Gaze Correction", 
                Symbol.View, 
                new List<string>() { "Off", "On" }, 
                controlPayload == null ? 0 : (int)controlPayload.Flags,
                0,
                EyeGazeCorrectionHandler);
            effect.IsEnabled = (null != controlPayload);
            UIPanelRows.Children.Add(effect);

            var effect2 = new EffectToggle();
            var customControlPayload = CustomCameraKsPropertyInquiry.GetAugmentedMediaSourceCustomControl(AugmentedMediaSourceCustomControlKind.KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_CUSTOMFX, m_controller);
            effect2.Initialize(
                "Custom FX", 
                Symbol.Emoji, 
                new List<string>() { "Off", "Auto" }, 
                customControlPayload == null ? 0 : (int)customControlPayload.Flags,
                0,
                CustomFXHandler);
            effect2.IsEnabled = (null != customControlPayload);
            UIPanelRows.Children.Add(effect2);

            var effect3 = new EffectToggle();
            var customControlPayload2 = CustomCameraKsPropertyInquiry.GetSimpleCustomControl(SimpleCustomControlKind.Coloring, m_controller);
            var colorModes = (ColorModeKind[])Enum.GetValues(typeof(ColorModeKind));
            var currentValueIndex = customControlPayload2 == null ? 0 : Array.FindIndex(colorModes, (color) => (UInt32)color == customControlPayload2.ColorMode);
            var defaultValueIndex = customControlPayload2 == null ? 0 : Array.FindIndex(colorModes, (color) => color == ColorModeKind.Grayscale);
            effect3.Initialize(
                "Coloring",
                Symbol.Highlight,
                Enum.GetNames(typeof(ColorModeKind)).ToList(),
                customControlPayload2 == null ? 0 : currentValueIndex,
                defaultValueIndex,
                ColoringControlHandler);
            effect3.IsEnabled = (null != customControlPayload2);
            UIPanelRows.Children.Add(effect3);
        }

        private void EyeGazeCorrectionHandler(object sender, SelectionChangedEventArgs e)
        {
            Debug.WriteLine("EyeGazeCorrectionHandler");
            PropertyInquiry.SetExtendedControlFlags(m_controller, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION, (ulong)(int)e.AddedItems[0]);
        }

        private void CustomFXHandler(object sender, SelectionChangedEventArgs e)
        {
            Debug.WriteLine("CustomFXHandler");
            CustomCameraKsPropertyInquiry.SetAugmentedMediaSourceCustomControlFlags(AugmentedMediaSourceCustomControlKind.KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_CUSTOMFX, m_controller, (UInt64)((int)e.AddedItems[0]));
        }

        private void ColoringControlHandler(object sender, SelectionChangedEventArgs e)
        {
            Debug.WriteLine("ColoringControlHandler");
            var colorModeKinds = Enum.GetValues(typeof(ColorModeKind));
            ColorModeKind selection = (ColorModeKind)colorModeKinds.GetValue((int)e.AddedItems[0]);
            CustomCameraKsPropertyInquiry.SetSimpleCustomControlFlags(SimpleCustomControlKind.Coloring, m_controller, (UInt32)selection);
        }
    }
}
