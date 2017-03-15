﻿using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Telegram.Api.Aggregator;
using Telegram.Api.Helpers;
using Telegram.Api.TL;
using Unigram.ViewModels;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Hosting;
using Windows.UI.Xaml.Media;

namespace Unigram.Controls
{
    public class BubbleListView : ListView
    {
        public DialogViewModel ViewModel => DataContext as DialogViewModel;

        public ScrollViewer ScrollingHost { get; private set; }
        public ItemsStackPanel ItemsStack { get; private set; }

        public BubbleListView()
        {
            DefaultStyleKey = typeof(ListView);

            Loaded += OnLoaded;
        }

        private void OnLoaded(object sender, RoutedEventArgs e)
        {
            var panel = ItemsPanelRoot as ItemsStackPanel;
            if (panel != null)
            {
                ItemsStack = panel;
                ItemsStack.ItemsUpdatingScrollMode = UpdatingScrollMode;
                ItemsStack.SizeChanged += Panel_SizeChanged;
            }
        }

        protected override void OnApplyTemplate()
        {
            ScrollingHost = (ScrollViewer)GetTemplateChild("ScrollViewer");
            ScrollingHost.ViewChanged += ScrollingHost_ViewChanged;

            base.OnApplyTemplate();
        }

        private async void Panel_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            if (ScrollingHost.ScrollableHeight < 120)
            {
                if (!ViewModel.IsFirstSliceLoaded)
                {
                    await ViewModel.LoadPreviousSliceAsync();
                }

                await ViewModel.LoadNextSliceAsync();
            }
        }

        private async void ScrollingHost_ViewChanged(object sender, ScrollViewerViewChangedEventArgs e)
        {
            if (ScrollingHost.VerticalOffset < 120 && !e.IsIntermediate)
            {
                await ViewModel.LoadNextSliceAsync();
            }
            else if (ScrollingHost.ScrollableHeight - ScrollingHost.VerticalOffset < 120 && !e.IsIntermediate)
            {
                if (ViewModel.IsFirstSliceLoaded == false)
                {
                    await ViewModel.LoadPreviousSliceAsync();
                }
            }
        }

        #region UpdatingScrollMode

        public ItemsUpdatingScrollMode UpdatingScrollMode
        {
            get { return (ItemsUpdatingScrollMode)GetValue(UpdatingScrollModeProperty); }
            set { SetValue(UpdatingScrollModeProperty, value); }
        }

        public static readonly DependencyProperty UpdatingScrollModeProperty =
            DependencyProperty.Register("UpdatingScrollMode", typeof(ItemsUpdatingScrollMode), typeof(BubbleListView), new PropertyMetadata(ItemsUpdatingScrollMode.KeepItemsInView, OnUpdatingScrollModeChanged));

        private static void OnUpdatingScrollModeChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            var sender = d as BubbleListView;
            if (sender.ItemsStack != null)
            {
                sender.ItemsStack.ItemsUpdatingScrollMode = (ItemsUpdatingScrollMode)e.NewValue;
            }
        }

        #endregion

        private int count;
        protected override DependencyObject GetContainerForItemOverride()
        {
            Debug.WriteLine($"New listview item: {++count}");
            return new BubbleListViewItem(this);
        }

        protected override void PrepareContainerForItemOverride(DependencyObject element, object item)
        {
            var bubble = element as BubbleListViewItem;
            var messageCommon = item as TLMessageCommonBase;

            if (bubble != null && messageCommon != null)
            {
                if (item is TLMessageService)
                {
                    bubble.Padding = new Thickness(4, 0, 4, 0);
                }
                else
                {
                    var message = item as TLMessage;
                    if (message != null && message.IsPost)
                    {
                        bubble.Padding = new Thickness(4, 0, 4, 0);
                    }
                    else
                    {
                        bubble.Padding = new Thickness(52, 0, 4, 0);
                    }
                }
            }

            base.PrepareContainerForItemOverride(element, item);
        }
    }
}