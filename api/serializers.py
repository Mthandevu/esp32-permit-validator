from rest_framework import serializers

from core.models import VerificationRecord, Driver


class BarcodeVerificationSerializer(serializers.Serializer):
	barcode = serializers.CharField(max_length=100, required=True)
	is_manual = serializers.BooleanField(required=True)


class DriverSerializer(serializers.ModelSerializer):
	full_name = serializers.CharField()
	gender = serializers.CharField(source="get_gender_display")
	is_permit_valid = serializers.BooleanField()
	valid_up_to = serializers.DateTimeField(format="%Y-%m-%d")
	censored_id = serializers.CharField()

	class Meta:
		model = Driver
		fields = '__all__'


class VerificationRecordSerializer(serializers.ModelSerializer):
	driver = DriverSerializer(read_only=True, many=False)
	created_at = serializers.DateTimeField(format="%Y-%m-%d %H:%M")

	class Meta:
		model = VerificationRecord
		fields = '__all__'
