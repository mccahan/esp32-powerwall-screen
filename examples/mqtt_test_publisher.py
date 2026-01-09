#!/usr/bin/env python3
"""
MQTT Test Publisher for ESP32 Powerwall Display

This script publishes random power data to MQTT topics for testing the display.

Requirements:
    pip install paho-mqtt

Usage:
    python mqtt_test_publisher.py --broker 192.168.1.100 --prefix pypowerwall
"""

import argparse
import time
import random
import paho.mqtt.client as mqtt

def generate_power_data():
    """Generate realistic power flow data"""
    # Solar varies throughout the day (0-5000W)
    solar = random.uniform(0, 5000)
    
    # Home consumption (500-3000W)
    home = random.uniform(500, 3000)
    
    # Battery level (0-100%)
    battery_level = random.uniform(20, 95)
    
    # Battery power (-3000 to +3000W)
    # Negative = charging, Positive = discharging
    battery = random.uniform(-2000, 2000)
    
    # Grid = home - solar - battery
    # Positive = importing, Negative = exporting
    grid = home - solar - battery
    
    return {
        'solar': solar,
        'home': home,
        'battery': battery,
        'battery_level': battery_level,
        'grid': grid
    }

def main():
    parser = argparse.ArgumentParser(description='Publish test data to MQTT for Powerwall Display')
    parser.add_argument('--broker', required=True, help='MQTT broker address')
    parser.add_argument('--port', type=int, default=1883, help='MQTT broker port')
    parser.add_argument('--prefix', default='pypowerwall', help='MQTT topic prefix')
    parser.add_argument('--user', default='', help='MQTT username')
    parser.add_argument('--password', default='', help='MQTT password')
    parser.add_argument('--interval', type=float, default=2.0, help='Update interval in seconds')
    
    args = parser.parse_args()
    
    # Create MQTT client
    client = mqtt.Client()
    
    # Set credentials if provided
    if args.user:
        client.username_pw_set(args.user, args.password)
    
    # Connect to broker
    print(f"Connecting to MQTT broker at {args.broker}:{args.port}...")
    client.connect(args.broker, args.port, 60)
    client.loop_start()
    
    print(f"Publishing to topics with prefix: {args.prefix}")
    print("Press Ctrl+C to stop\n")
    
    try:
        while True:
            data = generate_power_data()
            
            # Publish to topics
            client.publish(f"{args.prefix}/solar/instant_power", f"{data['solar']:.1f}")
            client.publish(f"{args.prefix}/load/instant_power", f"{data['home']:.1f}")
            client.publish(f"{args.prefix}/battery/instant_power", f"{data['battery']:.1f}")
            client.publish(f"{args.prefix}/battery/level", f"{data['battery_level']:.1f}")
            client.publish(f"{args.prefix}/site/instant_power", f"{data['grid']:.1f}")
            
            # Print status
            print(f"Solar: {data['solar']:7.1f}W | "
                  f"Grid: {data['grid']:7.1f}W | "
                  f"Battery: {data['battery']:7.1f}W ({data['battery_level']:.1f}%) | "
                  f"Home: {data['home']:7.1f}W")
            
            time.sleep(args.interval)
            
    except KeyboardInterrupt:
        print("\nStopping...")
        client.loop_stop()
        client.disconnect()

if __name__ == '__main__':
    main()
