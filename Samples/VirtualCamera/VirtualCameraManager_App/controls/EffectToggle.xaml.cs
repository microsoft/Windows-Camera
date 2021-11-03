// Copyright (C) Microsoft Corporation. All rights reserved.

using System.Collections.Generic;
using System.Threading;
using Windows.UI;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;

namespace VirtualCameraManager_App
{
    /// <summary>
    /// Class for a UI control to toggle an effect.
    /// </summary>
    public sealed partial class EffectToggle : UserControl
    {
        // members
        public event SelectionChangedEventHandler SelectionChanged;
        private SelectionChangedEventHandler m_handler;
        private List<string> m_possibleValues;
        private int m_offValueIndex = 0;
        private SemaphoreSlim m_lock = new SemaphoreSlim(1);
        
        // methods
        public EffectToggle()
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Initialize the toggle button with a set of possible values and an initial value, linking an interaction handler.
        /// </summary>
        /// <param name="title"></param>
        /// <param name="symbol"></param>
        /// <param name="possibleValues"></param>
        /// <param name="currentValueIndex"></param>
        /// <param name="offValueIndex"></param>
        /// <param name="handler"></param>
        public void Initialize(string title, Symbol symbol,  List<string> possibleValues, int currentValueIndex, int offValueIndex, SelectionChangedEventHandler handler)
        {
            m_lock.Wait();
            try
            {
                Clear();

                UITitle.Text = title;
                UISymbolIcon.Symbol = symbol;

                m_possibleValues = possibleValues;
                m_offValueIndex = offValueIndex;
                UIValueSelected.Text = m_possibleValues[currentValueIndex];

                SetActive(UIValueSelected.Text != m_possibleValues[m_offValueIndex]);

                var items = (UIButton.Flyout as MenuFlyout).Items;

                foreach (var possibleValue in m_possibleValues)
                {
                    var item = new MenuFlyoutItem()
                    {
                        Text = possibleValue
                    };
                    item.Click += MenuFlyoutItem_Click;
                    items.Add(item);
                }
                if (handler != null)
                {
                    m_handler = handler;
                    SelectionChanged += handler;
                }
            }
            finally
            {
                m_lock.Release();
            }
        }

        /// <summary>
        /// Signify the control is activated or not in the UI
        /// </summary>
        /// <param name="isActive"></param>
        private void SetActive(bool isActive)
        {
            if(isActive)
            {
                UIButton.BorderBrush = new SolidColorBrush(Colors.Yellow);
            }
            else
            {
                UIButton.BorderBrush = new SolidColorBrush(Colors.Transparent);
            }
        }

        private void Clear()
        {
            var items = (UIButton.Flyout as MenuFlyout).Items;
            items.Clear();

            if(SelectionChanged != null && m_handler != null)
            {
                SelectionChanged -= m_handler;
            }
        }

        private void MenuFlyoutItem_Click(object sender, RoutedEventArgs e)
        {
            var index = m_possibleValues.FindIndex(x => x == (sender as MenuFlyoutItem).Text);
            UIValueSelected.Text = m_possibleValues[index];

            SetActive(UIValueSelected.Text != m_possibleValues[m_offValueIndex]);

            if (SelectionChanged != null)
            {
                SelectionChanged.Invoke(this, new SelectionChangedEventArgs(new List<object>(), new List<object>() { index }));
            }
        }
    }
}
