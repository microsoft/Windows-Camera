// Copyright (C) Microsoft Corporation. All rights reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using VirtualCameraManager_WinRT;
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
            var controlPayload = CameraKsPropertyInquiry.GetExtendedControl(ExtendedControlKind.EyeGazeCorrection, m_controller);
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
            var customControlPayload = CameraKsPropertyInquiry.GetAugmentedMediaSourceCustomControl(AugmentedMediaSourceCustomControlKind.KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_CUSTOMFX, m_controller);
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
            var customControlPayload2 = CameraKsPropertyInquiry.GetSimpleCustomControl(SimpleCustomControlKind.Coloring, m_controller);
            var colorModes= Enum.GetValues(typeof(ColorModeKind)).Cast<ColorModeKind>().ToList();
            effect3.Initialize(
                "Coloring",
                Symbol.Highlight,
                Enum.GetNames(typeof(ColorModeKind)).ToList(),
                customControlPayload2 == null ? 0 : colorModes.IndexOf((ColorModeKind)customControlPayload2.ColorMode),
                colorModes.IndexOf(ColorModeKind.Grayscale),
                ColoringControlHandler);
            effect3.IsEnabled = (null != customControlPayload2);
            UIPanelRows.Children.Add(effect3);
        }

        private void EyeGazeCorrectionHandler(object sender, SelectionChangedEventArgs e)
        {
            Debug.WriteLine("EyeGazeCorrectionHandler");
            CameraKsPropertyInquiry.SetExtendedControlFlags(ExtendedControlKind.EyeGazeCorrection, m_controller, (ulong)(int)e.AddedItems[0]);
        }

        private void CustomFXHandler(object sender, SelectionChangedEventArgs e)
        {
            Debug.WriteLine("CustomFXHandler");
            CameraKsPropertyInquiry.SetAugmentedMediaSourceCustomControlFlags(AugmentedMediaSourceCustomControlKind.KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_CUSTOMFX, m_controller, (ulong)((int)e.AddedItems[0]));
        }

        private void ColoringControlHandler(object sender, SelectionChangedEventArgs e)
        {
            Debug.WriteLine("ColoringControlHandler");
            List<ColorModeKind> colorModeKinds = Enum.GetValues(typeof(ColorModeKind)).Cast<ColorModeKind>().ToList();
            ColorModeKind selection = colorModeKinds[(int)e.AddedItems[0]];
            CameraKsPropertyInquiry.SetSimpleCustomControlFlags(SimpleCustomControlKind.Coloring, m_controller, (ulong)selection);
        }
    }
}
